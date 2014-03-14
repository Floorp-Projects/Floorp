/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["WebappsHandler"];

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/WebappsInstaller.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");

this.WebappsHandler = {
  observe: function(subject, topic, data) {
    data = JSON.parse(data);
    data.mm = subject;

    switch (topic) {
      case "webapps-ask-install":
        let chromeWin = Services.wm.getOuterWindowWithId(data.oid);
        if (chromeWin)
          this.doInstall(data, chromeWin);
        break;
      case "webapps-launch":
        WebappOSUtils.launch(data);
        break;
      case "webapps-uninstall":
        WebappOSUtils.uninstall(data);
        break;
    }
  },

  doInstall: function(data, window) {
    let jsonManifest = data.isPackage ? data.app.updateManifest : data.app.manifest;
    let manifest = new ManifestHelper(jsonManifest, data.app.origin);
    let name = manifest.name;
    let bundle = Services.strings.createBundle("chrome://webapprt/locale/webapp.properties");

    let choice = Services.prompt.confirmEx(
      window,
      bundle.formatStringFromName("webapps.install.title", [name], 1),
      bundle.formatStringFromName("webapps.install.description", [name], 1),
      // Set both buttons to strings with the cancel button being default
      Ci.nsIPromptService.BUTTON_POS_1_DEFAULT |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_0 |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_1,
      bundle.GetStringFromName("webapps.install.install"),
      bundle.GetStringFromName("webapps.install.dontinstall"),
      null,
      null,
      {});

    // Perform the install if the user allows it
    if (choice == 0) {
      let shell = WebappsInstaller.init(data);

      if (shell) {
        let localDir = null;
        if (shell.appProfile) {
          localDir = shell.appProfile.localDir;
        }

        DOMApplicationRegistry.confirmInstall(data, localDir,
          function (aManifest) {
            WebappsInstaller.install(data, aManifest);
          }
        );
      } else {
        DOMApplicationRegistry.denyInstall(data);
      }
    } else {
      DOMApplicationRegistry.denyInstall(data);
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

Services.obs.addObserver(WebappsHandler, "webapps-ask-install", false);
Services.obs.addObserver(WebappsHandler, "webapps-launch", false);
Services.obs.addObserver(WebappsHandler, "webapps-uninstall", false);
