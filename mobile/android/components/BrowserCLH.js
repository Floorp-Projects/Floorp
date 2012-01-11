const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");


function dump(a) {
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

function openWindow(aParent, aURL, aTarget, aFeatures, aArgs) {
  let argString = null;
  if (aArgs && !(aArgs instanceof Ci.nsISupportsArray)) {
    argString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    argString.data = aArgs;
  }

  return Services.ww.openWindow(aParent, aURL, aTarget, aFeatures, argString || aArgs);
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
    let urlParam = "about:home";
    try {
        urlParam = aCmdLine.handleFlagWithParam("remote", false);
    } catch (e) { /* Optional */ }

    try {
      let uri = resolveURIInternal(aCmdLine, urlParam);
      if (!uri)
        return;

      let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
      if (browserWin) {
        browserWin.browserDOMWindow.openURI(uri, null, Ci.nsIBrowserDOMWindow.OPEN_NEWTAB, Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
      } else {
        browserWin = openWindow(null, "chrome://browser/content/browser.xul", "_blank", "chrome,dialog=no,all", urlParam);
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
