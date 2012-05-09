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
  let restoreSessionBool = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
  let pinnedBool = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
  let widthInt = Cc["@mozilla.org/supports-PRInt32;1"].createInstance(Ci.nsISupportsPRInt32);
  let heightInt = Cc["@mozilla.org/supports-PRInt32;1"].createInstance(Ci.nsISupportsPRInt32);

  if ("url" in aArgs) {
    urlString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    urlString.data = aArgs.url;
  }
  restoreSessionBool.data = "restoreSession" in aArgs ? aArgs.restoreSession : false;
  widthInt.data = "width" in aArgs ? aArgs.width : 1;
  heightInt.data = "height" in aArgs ? aArgs.height : 1;
  pinnedBool.data = "pinned" in aArgs ? aArgs.pinned : false;

  argsArray.AppendElement(urlString, false);
  argsArray.AppendElement(restoreSessionBool, false);
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

    let restoreSession = false;
    let width = 1;
    let height = 1;

    try {
      openURL = aCmdLine.handleFlagWithParam("url", false);
    } catch (e) { /* Optional */ }
    try {
      pinned = aCmdLine.handleFlag("webapp", false);
    } catch (e) { /* Optional */ }

    try {
      restoreSession = aCmdLine.handleFlag("restoresession", false);
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
        let whereFlags = pinned ? Ci.nsIBrowserDOMWindow.OPEN_SWITCHTAB : Ci.nsIBrowserDOMWindow.OPEN_NEWTAB;
        browserWin.browserDOMWindow.openURI(uri, null, whereFlags, Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
      } else {
        let args = {
          url: openURL,
          restoreSession: restoreSession,
          pinned: pinned,
          width: width,
          height: height
        };
        browserWin = openWindow(null, "chrome://browser/content/browser.xul", "_blank", "chrome,dialog=no,all", args);
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
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
