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

function openWindow(aParent, aURL, aTarget, aFeatures, aArgs) {
  let argsArray = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);
  let urlString = null;
  let pinnedBool = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
  let widthInt = Cc["@mozilla.org/supports-PRInt32;1"].createInstance(Ci.nsISupportsPRInt32);
  let heightInt = Cc["@mozilla.org/supports-PRInt32;1"].createInstance(Ci.nsISupportsPRInt32);

  if ("url" in aArgs) {
    urlString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    urlString.data = aArgs.url;
  }
  widthInt.data = "width" in aArgs ? aArgs.width : 1;
  heightInt.data = "height" in aArgs ? aArgs.height : 1;
  pinnedBool.data = "pinned" in aArgs ? aArgs.pinned : false;

  argsArray.AppendElement(urlString, false);
  argsArray.AppendElement(widthInt, false);
  argsArray.AppendElement(heightInt, false);
  argsArray.AppendElement(pinnedBool, false);
  return Services.ww.openWindow(aParent, aURL, aTarget, aFeatures, argsArray);
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

    let width = 1;
    let height = 1;

    try {
      openURL = aCmdLine.handleFlagWithParam("url", false);
    } catch (e) { /* Optional */ }
    try {
      pinned = aCmdLine.handleFlag("webapp", false);
    } catch (e) { /* Optional */ }

    try {
      width = aCmdLine.handleFlagWithParam("width", false);
    } catch (e) { /* Optional */ }
    try {
      height = aCmdLine.handleFlagWithParam("height", false);
    } catch (e) { /* Optional */ }

    try {
      let uri = resolveURIInternal(aCmdLine, openURL);
      if (!uri)
        return;

      let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
      if (browserWin) {
        if (!pinned) {
          browserWin.browserDOMWindow.openURI(uri, null, Ci.nsIBrowserDOMWindow.OPEN_NEWTAB, Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
        }
      } else {
        let args = {
          url: openURL,
          pinned: pinned,
          width: width,
          height: height
        };

        // Make sure webapps do not have: locationbar, personalbar, menubar, statusbar, and toolbar
        let flags = "chrome,dialog=no";
        if (!pinned)
          flags += ",all";

        browserWin = openWindow(null, "chrome://browser/content/browser.xul", "_blank", flags, args);
      }

      aCmdLine.preventDefault = true;
    } catch (x) {
      dump("BrowserCLH.handle: " + x);
    }
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  // XPCOMUtils factory
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}")
};

var components = [ BrowserCLH ];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
