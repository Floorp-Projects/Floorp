/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

Services.scriptloader
        .loadSubScript("chrome://webapprt/content/mochitest-shared.js", this);

// In test mode, the runtime isn't configured until we tell it to become
// an app, which requires us to use DOMApplicationRegistry to install one.
// But DOMApplicationRegistry needs to know the location of its registry dir,
// so we need to configure the runtime with at least that information.
WebappRT.config = {
  registryDir: Services.dirsvc.get("ProfD", Ci.nsIFile).path,
};


Cu.import("resource://gre/modules/Webapps.jsm");

DOMApplicationRegistry.allAppsLaunchable = true;

becomeWebapp("http://mochi.test:8888/tests/webapprt/test/content/test.webapp",
             undefined, function onBecome() {
  if (window.arguments && window.arguments[0]) {
    let testUrl = window.arguments[0].QueryInterface(Ci.nsIPropertyBag2).get("url");

    if (testUrl) {
      let win = Services.wm.getMostRecentWindow("webapprt:webapp");
      win.document.getElementById("content").setAttribute("src", testUrl);
    }
  }

  window.close();
});
