/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");


function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

function resolveURIInternal(aCmdLine, aArgument) {
  let uri = aCmdLine.resolveURI(aArgument);
  if (uri)
    return uri;

  try {
    let urifixup = Cc["@mozilla.org/docshell/urifixup;1"].getService(Ci.nsIURIFixup);
    uri = urifixup.createFixupURI(aArgument, 0);
  } catch (e) {
    Cu.reportError(e);
  }

  return uri;
}

function BrowserCLH() {}

BrowserCLH.prototype = {
  handle: function fs_handle(aCmdLine) {
    let openURL = "about:home";
    let pinned = false;

    try {
      openURL = aCmdLine.handleFlagWithParam("url", false);
    } catch (e) { /* Optional */ }

    if (!openURL) {
      // We don't have an URL on initial launch, and because the Fennec
      // CLH is not used for initial launch, we simply return here.
      return;
    }

    try {
      pinned = aCmdLine.handleFlag("bookmark", false);
    } catch (e) { /* Optional */ }

    try {
      let uri = resolveURIInternal(aCmdLine, openURL);
      if (!uri)
        return;

      let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
      if (browserWin) {
        let whereFlags = pinned ? Ci.nsIBrowserDOMWindow.OPEN_SWITCHTAB : Ci.nsIBrowserDOMWindow.OPEN_NEWTAB;
        browserWin.browserDOMWindow.openURI(uri, null, whereFlags, Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
      }

      aCmdLine.preventDefault = true;
    } catch (x) {
      dump("BrowserCLH.handle: " + x);
    }
  },

  /**
   * Register resource://android as the APK root.
   *
   * Consumers can access Android assets using resource://android/assets/FILENAME.
   */
  setResourceSubstitutions: function () {
    let registry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci["nsIChromeRegistry"]);
    // Like jar:jar:file:///data/app/org.mozilla.fennec-2.apk!/assets/omni.ja!/chrome/chrome/content/aboutHome.xhtml
    let url = registry.convertChromeURL(Services.io.newURI("chrome://browser/content/aboutHome.xhtml", null, null)).spec;
    // Like jar:file:///data/app/org.mozilla.fennec-2.apk!/
    url = url.substring(4, url.indexOf("!/") + 2);

    let protocolHandler = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);
    protocolHandler.setSubstitution("android", Services.io.newURI(url, null, null));
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case "app-startup":
        this.setResourceSubstitutions();
        break;
    }
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler,
                                         Ci.nsIObserver]),

  // XPCOMUtils factory
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}")
};

var components = [ BrowserCLH ];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
