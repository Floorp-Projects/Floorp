"use strict";

/* globals ExtensionAPI, Services, XPCOMUtils */

this.damp = class extends ExtensionAPI {
  getAPI(context) {
    return {
      damp: {
        startTest() {
          let { rootURI } = context.extension;

          // Some notes about using a DevTools loader for DAMP.
          //
          // 1. The DAMP loader needs to be same loader as the one used by the
          // toolbox later on. Otherwise, we will not retrieve the proper
          // instance of some modules.
          // The main devtools loader is the exported `loader` from Loader.jsm,
          // we have to use that.
          //
          // 2. The DAMP test path is dynamic, so we cannot hardcode the path
          // mapping in the DevTools loader. To workaround this, we set the path
          // in an env variable. This will then be read by Loader.jsm when it
          // creates the main DevTools loader. It is important to set this
          // before accessing the Loader for the first time.
          //
          // 3. DAMP contains at least one protocol test that loads a DAMP test
          // file in the content process (tests/server/protocol.js). In order
          // for the damp-test path mapping to work, Loaders in all processes
          // should be able to read the "damp test path" information. We use
          // use a custom preference to make the information available to
          // all processes.

          dump("[damp-api] Expose damp test path as a char preference\n");
          // dampTestPath points to testing/talos/talos/tests/devtools/addon/content/
          // (prefixed by the dynamically generated talos server)
          const dampTestPath = rootURI.resolve("content");
          Services.prefs.setCharPref("devtools.damp.test-path", dampTestPath);

          dump("[damp-api] Retrieve the main DevTools loader\n");
          const { loader, require } = ChromeUtils.import(
            "resource://devtools/shared/Loader.jsm"
          );

          // The loader should already support the damp-test path mapping.
          // We add two additional globals.
          loader.loader.globals.rootURI = rootURI;
          loader.loader.globals.dampWindow = context.appWindow;

          dump("[damp-api] Retrieve the DAMP runner and start the test\n");
          const { damp } = require("damp-test/damp");
          return damp.startTest();
        },
      },
    };
  }
};
