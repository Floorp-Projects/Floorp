
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");


function dump(a) {
  Cc["@mozilla.org/consoleservice;1"]
    .getService(Ci.nsIConsoleService)
    .logStringMessage(a);
}

function openWindow(aParent, aURL, aTarget, aFeatures, aArgs) {
  let argString = null;
  if (aArgs && !(aArgs instanceof Ci.nsISupportsArray)) {
    argString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    argString.data = aArgs;
  }

  return Services.ww.openWindow(aParent, aURL, aTarget, aFeatures, argString || aArgs);
}


function BrowserCLH() {}

BrowserCLH.prototype = {
  handle: function fs_handle(aCmdLine) {
    dump("fs_handle");

    let urlParam = aCmdLine.handleFlagWithParam("remote", false);
    if (!urlParam) {
      urlParam = "about:support";
      try {
        urlParam = Services.prefs.getCharPref("browser.last.uri");
      } catch (e) {};
    }
    dump("fs_handle: " + urlParam);

    try {
      let urifixup = Cc["@mozilla.org/docshell/urifixup;1"].getService(Ci.nsIURIFixup);
      let uri = urifixup.createFixupURI(urlParam, 1);
      if (!uri)
        return;
      dump("fs_handle: " + uri);

      let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
      if (browserWin) {
        browserWin.browserDOMWindow.openURI(uri,
                                            null,
                                            Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW,
                                            Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
      } else {
        browserWin = openWindow(null, "chrome://browser/content/browser.xul", "_blank", "chrome,dialog=no,all", urlParam);
      }

      aCmdLine.preventDefault = true;
      dump("fs_handle: done");
    } catch (x) {
      Cc["@mozilla.org/consoleservice;1"]
          .getService(Ci.nsIConsoleService)
          .logStringMessage("fs_handle exception!:  " + x);
    }
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  // XPCOMUtils factory
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}")
};

var components = [ BrowserCLH ];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
