Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const nsIAppShellService    = Components.interfaces.nsIAppShellService;
const nsISupports           = Components.interfaces.nsISupports;
const nsICategoryManager    = Components.interfaces.nsICategoryManager;
const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;
const nsICommandLine        = Components.interfaces.nsICommandLine;
const nsICommandLineHandler = Components.interfaces.nsICommandLineHandler;
const nsIFactory            = Components.interfaces.nsIFactory;
const nsIModule             = Components.interfaces.nsIModule;
const nsIWindowWatcher      = Components.interfaces.nsIWindowWatcher;

// CHANGEME: to the chrome URI of your extension or application
const CHROME_URI = "chrome://jsbridge/content/";

// CHANGEME: change the contract id, CID, and category to be unique
// to your application.
const clh_contractID = "@mozilla.org/commandlinehandler/general-startup;1?type=jsbridge";

// use uuidgen to generate a unique ID
const clh_CID = Components.ID("{2872d428-14f6-11de-ac86-001f5bd9235c}");

// category names are sorted alphabetically. Typical command-line handlers use a
// category that begins with the letter "m".
const clh_category = "jsbridge";

var aConsoleService = Components.classes["@mozilla.org/consoleservice;1"].
     getService(Components.interfaces.nsIConsoleService);

/**
 * Utility functions
 */

/**
 * Opens a chrome window.
 * @param aChromeURISpec a string specifying the URI of the window to open.
 * @param aArgument an argument to pass to the window (may be null)
 */
function openWindow(aChromeURISpec, aArgument)
{
  var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"].
    getService(Components.interfaces.nsIWindowWatcher);
  ww.openWindow(null, aChromeURISpec, "_blank",
                "chrome,menubar,toolbar,status,resizable,dialog=no",
                aArgument);
}

/**
 * The XPCOM component that implements nsICommandLineHandler.
 * It also implements nsIFactory to serve as its own singleton factory.
 */
function jsbridgeHandler() {
}
jsbridgeHandler.prototype = {
  classID: clh_CID,
  contractID: clh_contractID,
  classDescription: "jsbridgeHandler",
  _xpcom_categories: [{category: "command-line-handler", entry: clh_category}],

  /* nsISupports */
  QueryInterface : function clh_QI(iid)
  {
    if (iid.equals(nsICommandLineHandler) ||
        iid.equals(nsIFactory) ||
        iid.equals(nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsICommandLineHandler */

  handle : function clh_handle(cmdLine)
  {
    try {
      var port = cmdLine.handleFlagWithParam("jsbridge", false);
      if (port) {
        var server = {};
        Components.utils.import('resource://jsbridge/modules/server.js', server);
        server.startServer(parseInt(port));
      } else {
        var server = {};
        Components.utils.import('resource://jsbridge/modules/server.js', server);
        server.startServer(24242);
      }
    }
    catch (e) {
      Components.utils.reportError("incorrect parameter passed to -jsbridge on the command line.");
    }

  },

  // CHANGEME: change the help info as appropriate, but
  // follow the guidelines in nsICommandLineHandler.idl
  // specifically, flag descriptions should start at
  // character 24, and lines should be wrapped at
  // 72 characters with embedded newlines,
  // and finally, the string should end with a newline
  helpInfo : "  -jsbridge            Port to run jsbridge on.\n",

  /* nsIFactory */

  createInstance : function clh_CI(outer, iid)
  {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    return this.QueryInterface(iid);
  },

  lockFactory : function clh_lock(lock)
  {
    /* no-op */
  }
};

/**
 * XPCOMUtils.generateNSGetFactory was introduced in Mozilla 2 (Firefox 4).
 * XPCOMUtils.generateNSGetModule is for Mozilla 1.9.1 (Firefox 3.5).
 */
if (XPCOMUtils.generateNSGetFactory)
  const NSGetFactory = XPCOMUtils.generateNSGetFactory([jsbridgeHandler]);
else
  const NSGetModule = XPCOMUtils.generateNSGetModule([jsbridgeHandler]);
