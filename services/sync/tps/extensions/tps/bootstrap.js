/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "categoryManager",
                                   "@mozilla.org/categorymanager;1",
                                   "nsICategoryManager");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsISubstitutingProtocolHandler");

const Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

const CATMAN_CONTRACTID = "@mozilla.org/categorymanager;1";

const CATEGORY_NAME = "command-line-handler";
const CATEGORY_ENTRY = "m-tps";

function TPSCmdLine() {}

TPSCmdLine.prototype = {
  factory: XPCOMUtils._getFactory(TPSCmdLine),
  classDescription: "TPSCmdLine",
  classID: Components.ID("{4e5bd3f0-41d3-11df-9879-0800200c9a66}"),
  contractID: "@mozilla.org/commandlinehandler/general-startup;1?type=tps",

  QueryInterface:   ChromeUtils.generateQI([Ci.nsICommandLineHandler]),

  register() {
    Cm.registerFactory(this.classID, this.classDescription,
                       this.contractID, this.factory);

    categoryManager.addCategoryEntry(CATEGORY_NAME, CATEGORY_ENTRY,
                                     this.contractID, false, true);
  },

  unregister() {
    categoryManager.deleteCategoryEntry(CATEGORY_NAME, CATEGORY_ENTRY,
                                        this.contractID, false);

    Cm.unregisterFactory(this.classID, this.factory);
  },

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
    let uri = cmdLine.resolveURI(OS.Path.normalize(uristr)).spec;

    const onStartupFinished = () => {
      Services.obs.removeObserver(onStartupFinished, "browser-delayed-startup-finished");
      /* Ignore the platform's online/offline status while running tests. */
      Services.io.manageOfflineStatus = false;
      Services.io.offline = false;
      ChromeUtils.import("resource://tps/tps.jsm");
      ChromeUtils.import("resource://tps/quit.js", TPS);
      TPS.RunTestPhase(uri, phase, logfile, options).catch(err => TPS.DumpError("TestPhase failed", err));
    };
    Services.obs.addObserver(onStartupFinished, "browser-delayed-startup-finished");
  },

  helpInfo: "  --tps <file>              Run TPS tests with the given test file.\n" +
            "  --tpsphase <phase>        Run the specified phase in the TPS test.\n" +
            "  --tpslogfile <file>       Logfile for TPS output.\n" +
            "  --ignore-unused-engines   Don't load engines not used in tests.\n",
};

function startup(data, reason) {
  TPSCmdLine.prototype.register();
  resProto.setSubstitution("tps", Services.io.newURI("resource/", null, data.resourceURI));
}

function shutdown(data, reason) {
  resProto.setSubstitution("tps", null);
  TPSCmdLine.prototype.unregister();
}

function install(data, reason) {}
function uninstall(data, reason) {}
