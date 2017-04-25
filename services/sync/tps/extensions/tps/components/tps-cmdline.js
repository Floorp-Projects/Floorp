/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CC = Components.classes;
const CI = Components.interfaces;

const TPS_ID                         = "tps@mozilla.org";
const TPS_CMDLINE_CONTRACTID         = "@mozilla.org/commandlinehandler/general-startup;1?type=tps";
const TPS_CMDLINE_CLSID              = Components.ID("{4e5bd3f0-41d3-11df-9879-0800200c9a66}");
const CATMAN_CONTRACTID              = "@mozilla.org/categorymanager;1";
const nsISupports                    = Components.interfaces.nsISupports;

const nsICategoryManager             = Components.interfaces.nsICategoryManager;
const nsICmdLineHandler              = Components.interfaces.nsICmdLineHandler;
const nsICommandLine                 = Components.interfaces.nsICommandLine;
const nsICommandLineHandler          = Components.interfaces.nsICommandLineHandler;
const nsIComponentRegistrar          = Components.interfaces.nsIComponentRegistrar;
const nsISupportsString              = Components.interfaces.nsISupportsString;
const nsIWindowWatcher               = Components.interfaces.nsIWindowWatcher;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function TPSCmdLineHandler() {}

TPSCmdLineHandler.prototype = {
  classDescription: "TPSCmdLineHandler",
  classID: TPS_CMDLINE_CLSID,
  contractID: TPS_CMDLINE_CONTRACTID,

  QueryInterface: XPCOMUtils.generateQI([nsISupports,
                                         nsICommandLineHandler,
                                         nsICmdLineHandler]),   /* nsISupports */

  /* nsICmdLineHandler */
  commandLineArgument: "-tps",
  prefNameForStartup: "general.startup.tps",
  helpText: "Run TPS tests with the given test file.",
  handlesArgs: true,
  defaultArgs: "",
  openWindowWithArgs: true,

  /* nsICommandLineHandler */
  handle: function handler_handle(cmdLine) {
    let options = {};

    let uristr = cmdLine.handleFlagWithParam("tps", false);
    if (uristr == null)
        return;
    let phase = cmdLine.handleFlagWithParam("tpsphase", false);
    if (phase == null)
        throw Error("must specify --tpsphase with --tps");
    let logfile = cmdLine.handleFlagWithParam("tpslogfile", false);
    if (logfile == null)
        logfile = "";

    options.ignoreUnusedEngines = cmdLine.handleFlag("ignore-unused-engines",
                                                     false);


    /* Ignore the platform's online/offline status while running tests. */
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
              .getService(Components.interfaces.nsIIOService2);
    ios.manageOfflineStatus = false;
    ios.offline = false;

    Components.utils.import("resource://tps/tps.jsm");
    Components.utils.import("resource://tps/quit.js", TPS);
    let uri = cmdLine.resolveURI(uristr).spec;
    TPS.RunTestPhase(uri, phase, logfile, options);

    // cmdLine.preventDefault = true;
  },

  helpInfo: "  --tps <file>              Run TPS tests with the given test file.\n" +
            "  --tpsphase <phase>        Run the specified phase in the TPS test.\n" +
            "  --tpslogfile <file>       Logfile for TPS output.\n" +
            "  --ignore-unused-engines   Don't load engines not used in tests.\n",
};


var TPSCmdLineFactory = {
  createInstance(outer, iid) {
    if (outer != null) {
      throw new Error(Components.results.NS_ERROR_NO_AGGREGATION);
    }

    return new TPSCmdLineHandler().QueryInterface(iid);
  }
};


var TPSCmdLineModule = {
  registerSelf(compMgr, fileSpec, location, type) {
    compMgr = compMgr.QueryInterface(nsIComponentRegistrar);

    compMgr.registerFactoryLocation(TPS_CMDLINE_CLSID,
                                    "TPS CommandLine Service",
                                    TPS_CMDLINE_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

    var catman = Components.classes[CATMAN_CONTRACTID].getService(nsICategoryManager);
    catman.addCategoryEntry("command-line-argument-handlers",
                            "TPS command line handler",
                            TPS_CMDLINE_CONTRACTID, true, true);
    catman.addCategoryEntry("command-line-handler",
                            "m-tps",
                            TPS_CMDLINE_CONTRACTID, true, true);
  },

  unregisterSelf(compMgr, fileSpec, location) {
    compMgr = compMgr.QueryInterface(nsIComponentRegistrar);

    compMgr.unregisterFactoryLocation(TPS_CMDLINE_CLSID, fileSpec);
    let catman = Components.classes[CATMAN_CONTRACTID].getService(nsICategoryManager);
    catman.deleteCategoryEntry("command-line-argument-handlers",
                               "TPS command line handler", true);
    catman.deleteCategoryEntry("command-line-handler",
                               "m-tps", true);
  },

  getClassObject(compMgr, cid, iid) {
    if (cid.equals(TPS_CMDLINE_CLSID)) {
      return TPSCmdLineFactory;
    }

    if (!iid.equals(Components.interfaces.nsIFactory)) {
      throw new Error(Components.results.NS_ERROR_NOT_IMPLEMENTED);
    }

    throw new Error(Components.results.NS_ERROR_NO_INTERFACE);
  },

  canUnload(compMgr) {
    return true;
  }
};

/**
* XPCOMUtils.generateNSGetFactory was introduced in Mozilla 2 (Firefox 4).
* XPCOMUtils.generateNSGetModule is for Mozilla 1.9.2 (Firefox 3.6).
*/
if (XPCOMUtils.generateNSGetFactory)
    var NSGetFactory = XPCOMUtils.generateNSGetFactory([TPSCmdLineHandler]);

function NSGetModule(compMgr, fileSpec) {
  return TPSCmdLineModule;
}
