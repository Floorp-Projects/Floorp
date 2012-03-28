/* ***** BEGIN LICENSE BLOCK *****
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ***** END LICENSE BLOCK ***** */

Cu.import("resource://gre/modules/Webapps.jsm");

let WebappsUI = {
  init: function() {
    Services.obs.addObserver(this, "webapps-ask-install", false);
    Services.obs.addObserver(this, "webapps-launch", false);
  },
  
  uninit: function() {
    Services.obs.removeObserver(this, "webapps-ask-install");
    Services.obs.removeObserver(this, "webapps-launch");
  },
  
  observe: function(aSubject, aTopic, aData) {
    let data = JSON.parse(aData);
    switch (aTopic) {
      case "webapps-ask-install":
        this.doInstall(data);
        break;
      case "webapps-launch":
        DOMApplicationRegistry.getManifestFor(data.origin, (function(aManifest) {
	   if (!aManifest)
	     return;
          let manifest = new DOMApplicationManifest(aManifest, data.origin);
          this.openURL(manifest.fullLaunchPath(), data.origin);
        }).bind(this));
        break;
    }
  },
  
  doInstall: function(aData) {
    let manifest = new DOMApplicationManifest(aData.app.manifest, aData.app.origin);
    let name = manifest.name ? manifest.name : manifest.fullLaunchPath();
    if (Services.prompt.confirm(null, Strings.browser.GetStringFromName("webapps.installTitle"), name))
      DOMApplicationRegistry.confirmInstall(aData);
  },
  
  openURL: function(aURI, aOrigin) {
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

    let tabs = BrowserApp.tabs;
    let tab = null;
    for (let i = 0; i < tabs.length; i++) {
      let appOrigin = ss.getTabValue(tabs[i], "appOrigin");
      if (appOrigin == aOrigin)
        tab = tabs[i];
    }

    if (tab) {
      BrowserApp.selectTab(tab);
    } else {
      tab = BrowserApp.addTab(aURI);
      ss.setTabValue(tab, "appOrigin", aOrigin);
    }
  }
}
