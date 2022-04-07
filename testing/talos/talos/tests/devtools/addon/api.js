"use strict";

/* globals ExtensionAPI, Services, XPCOMUtils */

this.damp = class extends ExtensionAPI {
  getAPI(context) {
    return {
      damp: {
        startTest() {
          // Some notes about using a DevTools loader for DAMP.
          //
          // The DAMP loader needs to be same loader as the one used by the
          // toolbox later on. Otherwise, we will not retrieve the proper
          // instance of some modules.
          // The main devtools loader is the exported `loader` from Loader.jsm,
          // we have to use that.

          dump("[damp-api] Retrieve the main DevTools loader\n");
          const { loader, require } = ChromeUtils.import(
            "resource://devtools/shared/loader/Loader.jsm"
          );

          const { rootURI } = context.extension;
          const dampRootDir = rootURI.QueryInterface(Ci.nsIFileURL);

          const protocolHandler = Services.io
            .getProtocolHandler("resource")
            .QueryInterface(Ci.nsIResProtocolHandler);

          // Serve testing/talos/talos/tests/devtools/addon/ via "resource://damp-test"
          // Loader.jsm will map `require("damp-test/...")` to `resource://damp-test/content/...`
          // Thus allowing to load damp files from the content folder via the DevTools loader.
          protocolHandler.setSubstitution("damp-test", dampRootDir);

          // Expose the window to modules loaded for DAMP.
          loader.loader.globals.dampWindow = context.appWindow;

          dump("[damp-api] Retrieve the DAMP runner and start the test\n");
          const { damp } = require("damp-test/damp");
          return damp.startTest();
        },
      },
    };
  }
};
