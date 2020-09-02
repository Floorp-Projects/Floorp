/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Constants

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const CONTENT_HANDLING_URL = "chrome://mozapps/content/handling/dialog.xhtml";
const STRINGBUNDLE_URL = "chrome://mozapps/locale/handling/handling.properties";

// nsContentDispatchChooser class

function nsContentDispatchChooser() {}

nsContentDispatchChooser.prototype = {
  classID: Components.ID("e35d5067-95bc-4029-8432-e8f1e431148d"),

  // nsIContentDispatchChooser

  ask: function ask(aHandler, aURI, aPrincipal, aBrowsingContext, aReason) {
    var bundle = Services.strings.createBundle(STRINGBUNDLE_URL);

    let strings = [
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

    if (aBrowsingContext) {
      if (!aBrowsingContext.topChromeWindow) {
        Cu.reportError(
          "Can't show external protocol dialog. BrowsingContext has no chrome window associated."
        );
        return;
      }

      this._openTabDialog(
        strings,
        aHandler,
        aURI,
        aPrincipal,
        aBrowsingContext
      );
      return;
    }

    // If we don't have a BrowsingContext, we need to show a standalone window.
    this._openWindowDialog(
      strings,
      aHandler,
      aURI,
      aPrincipal,
      aBrowsingContext
    );
  },

  _openWindowDialog(strings, aHandler, aURI, aPrincipal, aBrowsingContext) {
    let params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    let SupportsString = Components.Constructor(
      "@mozilla.org/supports-string;1",
      "nsISupportsString"
    );
    for (let text of strings) {
      let string = new SupportsString();
      string.data = text;
      params.appendElement(string);
    }
    params.appendElement(aHandler);
    params.appendElement(aURI);
    params.appendElement(aPrincipal);
    params.appendElement(aBrowsingContext);

    Services.ww.openWindow(
      null,
      CONTENT_HANDLING_URL,
      null,
      "chrome,dialog=yes,resizable,centerscreen",
      params
    );
  },

  _openTabDialog(strings, aHandler, aURI, aPrincipal, aBrowsingContext) {
    let window = aBrowsingContext.topChromeWindow;

    let tabDialogBox = window.gBrowser.getTabDialogBox(
      aBrowsingContext.embedderElement
    );

    tabDialogBox.open(
      CONTENT_HANDLING_URL,
      {
        features: "resizable=yes",
        allowDuplicateDialogs: false,
        keepOpenSameOriginNav: true,
      },
      ...strings,
      aHandler,
      aURI,
      aPrincipal,
      aBrowsingContext
    );
  },

  // nsISupports

  QueryInterface: ChromeUtils.generateQI(["nsIContentDispatchChooser"]),
};

var EXPORTED_SYMBOLS = ["nsContentDispatchChooser"];
