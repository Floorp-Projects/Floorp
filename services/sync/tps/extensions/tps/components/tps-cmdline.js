/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TPS.
 *
 * The Initial Developer of the Original Code is
 * Christopher A. Aillon <christopher@aillon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
 *   L. David Baron, Mozilla Corporation <dbaron@dbaron.org> (modified for reftest)
 *   Jonathan Griffin <jgriffin@mozilla.com> (modified for TPS)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const CC = Components.classes;
const CI = Components.interfaces;

const TPS_ID                         = "tps@mozilla.org";
const TPS_CMDLINE_CONTRACTID         = "@mozilla.org/commandlinehandler/general-startup;1?type=tps";
const TPS_CMDLINE_CLSID              = Components.ID('{4e5bd3f0-41d3-11df-9879-0800200c9a66}');
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
TPSCmdLineHandler.prototype =
{
  classDescription: "TPSCmdLineHandler",
  classID         : TPS_CMDLINE_CLSID,
  contractID      : TPS_CMDLINE_CONTRACTID,

  QueryInterface: XPCOMUtils.generateQI([nsISupports,
                                         nsICommandLineHandler,
                                         nsICmdLineHandler]),   /* nsISupports */

  /* nsICmdLineHandler */
  commandLineArgument : "-tps",
  prefNameForStartup : "general.startup.tps",
  helpText : "Run TPS tests with the given test file.",
  handlesArgs : true,
  defaultArgs : "",
  openWindowWithArgs : true,

  /* nsICommandLineHandler */
  handle : function handler_handle(cmdLine) {
    var uristr = cmdLine.handleFlagWithParam("tps", false);
    if (uristr == null)
        return;
    var phase = cmdLine.handleFlagWithParam("tpsphase", false);
    if (phase == null)
        throw("must specify --tpsphase with --tps");
    var logfile = cmdLine.handleFlagWithParam("tpslogfile", false);
    if (logfile == null)
        logfile = "";

    /* Ignore the platform's online/offline status while running tests. */
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
              .getService(Components.interfaces.nsIIOService2);
    ios.manageOfflineStatus = false;
    ios.offline = false;

    Components.utils.import("resource://tps/tps.jsm");
    Components.utils.import("resource://tps/quit.js", TPS);
    let uri = cmdLine.resolveURI(uristr).spec;
    TPS.RunTestPhase(uri, phase, logfile);
    
    //cmdLine.preventDefault = true;
  },

  helpInfo : "  -tps <file>               Run TPS tests with the given test file.\n" +
             "  -tpsphase <phase>         Run the specified phase in the TPS test.\n" +
             "  -tpslogfile <file>        Logfile for TPS output.\n",
};


var TPSCmdLineFactory =
{
  createInstance : function(outer, iid)
  {
    if (outer != null) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }

    return new TPSCmdLineHandler().QueryInterface(iid);
  }
};


var TPSCmdLineModule =
{
  registerSelf : function(compMgr, fileSpec, location, type)
  {
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

  unregisterSelf : function(compMgr, fileSpec, location)
  {
    compMgr = compMgr.QueryInterface(nsIComponentRegistrar);

    compMgr.unregisterFactoryLocation(TPS_CMDLINE_CLSID, fileSpec);
    catman = Components.classes[CATMAN_CONTRACTID].getService(nsICategoryManager);
    catman.deleteCategoryEntry("command-line-argument-handlers",
                               "TPS command line handler", true);
    catman.deleteCategoryEntry("command-line-handler",
                               "m-tps", true);
  },

  getClassObject : function(compMgr, cid, iid)
  {
    if (cid.equals(TPS_CMDLINE_CLSID)) {
      return TPSCmdLineFactory;
    }

    if (!iid.equals(Components.interfaces.nsIFactory)) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  canUnload : function(compMgr)
  {
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
