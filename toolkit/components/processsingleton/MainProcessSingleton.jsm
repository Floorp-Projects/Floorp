/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function MainProcessSingleton() {}
MainProcessSingleton.prototype = {
  classID: Components.ID("{0636a680-45cb-11e4-916c-0800200c9a66}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  observe(subject, topic, data) {
    switch (topic) {
      case "app-startup": {
        Services.obs.addObserver(this, "ipc:first-content-process-created");

        ChromeUtils.import(
          "resource://gre/modules/CustomElementsListener.jsm",
          null
        );

        // Load this script early so that console.* is initialized
        // before other frame scripts.
        Services.mm.loadFrameScript(
          "chrome://global/content/browser-content.js",
          true,
          true
        );
        Services.ppmm.loadProcessScript(
          "chrome://global/content/process-content.js",
          true
        );
        Services.ppmm.loadProcessScript(
          "resource:///modules/ContentObservers.js",
          true
        );
        break;
      }

      case "ipc:first-content-process-created": {
        // L10nRegistry needs to be initialized before any content
        // process is loaded to populate the registered FileSource
        // categories.
        ChromeUtils.import("resource://gre/modules/L10nRegistry.jsm");
        break;
      }
    }
  },
};

var EXPORTED_SYMBOLS = ["MainProcessSingleton"];
