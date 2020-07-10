/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Constants

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const CONTENT_HANDLING_URL = "chrome://mozapps/content/handling/dialog.xhtml";
const STRINGBUNDLE_URL = "chrome://mozapps/locale/handling/handling.properties";

// A map from (top) browsing contexts that have opened dialogs, to the dialogs
// they have opened. We add items when new dialogs are opened, and remove them
// again when the browser navigates or the browsing context is destroyed.
const gContextsToDialogs = new WeakMap();

// A map from (top) browsing contexts to webprogress listeners. This is used
// because we have one listener per browser, and need to be able to throw it
// away again when we're done.
const gBoundWPLMap = new WeakMap();

class BoundProgresslistener {
  constructor(browser) {
    this.browsingContext = browser.browsingContext;
    this._knownPrincipal = browser.contentPrincipal;
  }

  onLocationChange(webProgress, channel, uri, flags) {
    if (
      webProgress.isTopLevel &&
      !(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)
    ) {
      // Close the dialog and unregister if we navigated to a different origin.
      // This keeps the dialog open for navigation on the same origin.
      let isPrivate = this.browsingContext.usePrivateBrowsing;
      if (!uri || !this._knownPrincipal.isSameOrigin(uri, isPrivate)) {
        gChooserTracker.unregisterContext(this.browsingContext);
      }
    }
  }
}

const gChooserTracker = {
  getDialogFor(browsingContext) {
    return gContextsToDialogs.get(browsingContext);
  },

  observe(aSubject, aTopic, aData) {
    if (
      aTopic == "dom-window-destroyed" &&
      aSubject.document.URL == CONTENT_HANDLING_URL
    ) {
      let browsingContext = aSubject.arguments[10];
      this.unregisterContext(browsingContext?.top);
    }

    if (aTopic == "browsing-context-discarded") {
      let browsingContext = aSubject;
      // Close the dialog if the tab is closed.
      if (browsingContext && browsingContext == browsingContext.top) {
        this.unregisterContext(browsingContext);
      }
    }
  },

  newDialogForContext(browsingContext, dialog) {
    if (!browsingContext) {
      return;
    }
    gContextsToDialogs.set(browsingContext, dialog);
    // Ensure we notice if a dialog goes away.
    if (!this._haveObserver) {
      Services.obs.addObserver(this, "dom-window-destroyed", true);
      Services.obs.addObserver(this, "browsing-context-discarded", true);
      this._haveObserver = true;
    }
    // Ensure we notice navigation:
    let browser = browsingContext.embedderElement;
    if (!browser) {
      return;
    }

    let listener = new BoundProgresslistener(browser);
    // Need to wrap it in a filter instance because the web progress listener
    // stuff uses weak references and drops our listener on the floor otherwise.
    let filter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);
    filter.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_LOCATION);
    gBoundWPLMap.set(browsingContext, filter);
    browser.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_LOCATION);
  },

  unregisterContext(browsingContext) {
    if (!browsingContext) {
      return;
    }
    let dialog = gContextsToDialogs.get(browsingContext);
    if (!dialog) {
      return;
    }
    if (!dialog.closed) {
      dialog.close();
    }

    // Remove the progress listener again, if we still have a browser
    // reference (we might not if the browsingContext is going away).
    let browser = browsingContext.embedderElement;
    if (browser) {
      browser.removeProgressListener(gBoundWPLMap.get(browsingContext));
    }

    gContextsToDialogs.delete(browsingContext);
    gBoundWPLMap.delete(browsingContext);
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
};

// nsContentDispatchChooser class

function nsContentDispatchChooser() {}

nsContentDispatchChooser.prototype = {
  classID: Components.ID("e35d5067-95bc-4029-8432-e8f1e431148d"),

  // nsIContentDispatchChooser

  ask: function ask(aHandler, aURI, aPrincipal, aBrowsingContext, aReason) {
    let window = aBrowsingContext?.topChromeWindow || null;

    if (aBrowsingContext) {
      const topBC = aBrowsingContext.top;
      // Do not allow more than 1 simultaneous dialog for the same originating
      // toplevel BC.
      let existingDialog = gChooserTracker.getDialogFor(topBC);
      if (existingDialog) {
        existingDialog.focus();
        return;
      }
    }

    var bundle = Services.strings.createBundle(STRINGBUNDLE_URL);

    // TODO when this is hooked up for content, we will need different strings
    //      for most of these
    var arr = [
      bundle.GetStringFromName("protocol.title"),
      "",
      bundle.GetStringFromName("protocol.description"),
      bundle.GetStringFromName("protocol.choices.label"),
      bundle.formatStringFromName("protocol.checkbox.label", [aURI.scheme]),
      bundle.GetStringFromName("protocol.checkbox.accesskey"),
      bundle.formatStringFromName("protocol.checkbox.extra", [
        Services.appinfo.name,
      ]),
    ];

    var params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    let SupportsString = Components.Constructor(
      "@mozilla.org/supports-string;1",
      "nsISupportsString"
    );
    for (let text of arr) {
      let string = new SupportsString();
      string.data = text;
      params.appendElement(string);
    }
    params.appendElement(aHandler);
    params.appendElement(aURI);
    params.appendElement(aPrincipal);
    params.appendElement(aBrowsingContext);

    let dialog = Services.ww.openWindow(
      window,
      CONTENT_HANDLING_URL,
      null,
      "chrome,dialog=yes,resizable,centerscreen",
      params
    );
    gChooserTracker.newDialogForContext(aBrowsingContext?.top, dialog);
  },

  // nsISupports

  QueryInterface: ChromeUtils.generateQI(["nsIContentDispatchChooser"]),
};

var EXPORTED_SYMBOLS = ["nsContentDispatchChooser"];
