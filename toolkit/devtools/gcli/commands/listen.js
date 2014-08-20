/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const Services = require("Services");
const gcli = require("gcli/index");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DevToolsLoader",
  "resource://gre/modules/devtools/Loader.jsm");

const BRAND_SHORT_NAME = Cc["@mozilla.org/intl/stringbundle;1"]
                           .getService(Ci.nsIStringBundleService)
                           .createBundle("chrome://branding/locale/brand.properties")
                           .GetStringFromName("brandShortName");

XPCOMUtils.defineLazyGetter(this, "debuggerServer", () => {
  // Create a separate loader instance, so that we can be sure to receive
  // a separate instance of the DebuggingServer from the rest of the
  // devtools.  This allows us to safely use the tools against even the
  // actors and DebuggingServer itself, especially since we can mark
  // serverLoader as invisible to the debugger (unlike the usual loader
  // settings).
  let serverLoader = new DevToolsLoader();
  serverLoader.invisibleToDebugger = true;
  serverLoader.main("devtools/server/main");
  let debuggerServer = serverLoader.DebuggerServer;
  debuggerServer.init();
  debuggerServer.addBrowserActors();
  return debuggerServer;
});

exports.items = [
  {
    name: "listen",
    description: gcli.lookup("listenDesc"),
    manual: gcli.lookupFormat("listenManual2", [ BRAND_SHORT_NAME ]),
    params: [
      {
        name: "port",
        type: "number",
        get defaultValue() {
          return Services.prefs.getIntPref("devtools.debugger.chrome-debugging-port");
        },
        description: gcli.lookup("listenPortDesc"),
      }
    ],
    exec: function(args, context) {
      var reply = debuggerServer.openListener(args.port);
      if (!reply) {
        throw new Error(gcli.lookup("listenDisabledOutput"));
      }

      if (debuggerServer.initialized) {
        return gcli.lookupFormat("listenInitOutput", [ "" + args.port ]);
      }

      return gcli.lookup("listenNoInitOutput");
    },
  }
];
