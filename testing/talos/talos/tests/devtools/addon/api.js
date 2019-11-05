"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

/* globals ExtensionAPI */
this.damp = class extends ExtensionAPI {
  getAPI(context) {
    return {
      damp: {
        startTest() {
          let { rootURI } = context.extension;
          let window = context.xulWindow;
          if (!("Damp" in window)) {
            let script = rootURI.resolve("content/damp.js");
            Services.scriptloader.loadSubScript(script, window);
          }

          let damp = new window.Damp();
          return damp.startTest(rootURI);
        },
      },
    };
  }
};
