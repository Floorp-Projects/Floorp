/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/commonjs/promise/core.js");

function pref(name, value) {
  return {
    name: name,
    value: value
  }
}

let WebAppRT = {
  DEFAULT_PREFS_FILENAME: "default-prefs.js",

  prefs: [
    // Disable all add-on locations other than the profile (which can't be disabled this way)
    pref("extensions.enabledScopes", 1),
    // Auto-disable any add-ons that are "dropped in" to the profile
    pref("extensions.autoDisableScopes", 1),
    // Disable add-on installation via the web-exposed APIs
    pref("xpinstall.enabled", false),
    // Set a future policy version to avoid the telemetry prompt.
    pref("toolkit.telemetry.prompted", 999),
    pref("toolkit.telemetry.notifiedOptOut", 999)
  ],

  init: function(isUpdate, url) {
    this.deck = document.getElementById("browsers");
    this.deck.addEventListener("click", this, false, true);

    // on first run, update any prefs
    if (isUpdate == "new") {
      this.getDefaultPrefs().forEach(this.addPref);

      // prevent offering to use helper apps for things that this app handles
      // i.e. don't show the "Open in market?" popup when we're showing the market app
      let uri = Services.io.newURI(url, null, null);
      Services.perms.add(uri, "native-intent", Ci.nsIPermissionManager.DENY_ACTION);
      Services.perms.add(uri, "offline-app", Ci.nsIPermissionManager.ALLOW_ACTION);
      Services.perms.add(uri, "indexedDB", Ci.nsIPermissionManager.ALLOW_ACTION);
      Services.perms.add(uri, "indexedDB-unlimited", Ci.nsIPermissionManager.ALLOW_ACTION);

      // update the blocklist url to use a different app id
      let blocklist = Services.prefs.getCharPref("extensions.blocklist.url");
      blocklist = blocklist.replace(/%APP_ID%/g, "webapprt-mobile@mozilla.org");
      Services.prefs.setCharPref("extensions.blocklist.url", blocklist);
    }

    return this.findManifestUrlFor(url);
  },

  findManifestUrlFor: function(aUrl) {
    let deferred = Promise.defer();
    let request = navigator.mozApps.mgmt.getAll();
    request.onsuccess = function() {
      let apps = request.result;
      for (let i = 0; i < apps.length; i++) {
        let app = apps[i];
        let manifest = new ManifestHelper(app.manifest, app.origin);

        // First see if this url matches any manifests we have registered
        // If so, get the launchUrl from the manifest and we'll launch with that
        //let app = DOMApplicationRegistry.getAppByManifestURL(aUrl);
        if (app.manifestURL == aUrl) {
          BrowserApp.manifestUrl = aUrl;
          deferred.resolve(manifest.fullLaunchPath());
          return;
        }
    
        // Otherwise, see if the apps launch path is this url
        if (manifest.fullLaunchPath() == aUrl) {
          // remove the old shortuct
          sendMessageToJava({
            gecko: {
              type: "Shortcut:Remove",
              title: manifest.name,
              url: manifest.fullLaunchPath(),
              origin: app.origin,
              shortcutType: "webapp"
            }
          });
          WebappsUI.createShortcut(manifest.name, manifest.fullLaunchPath(),
                                   WebappsUI.getBiggestIcon(manifest.icons, app.origin), "webapp");

          BrowserApp.manifestUrl = app.manifestURL;
          deferred.resolve(aUrl);
          return;
        }
      }
      deferred.reject("");
    };

    request.onerror = function() {
      deferred.reject("");
    };

    return deferred.promise;
  },

  getDefaultPrefs: function() {
    // read default prefs from the disk
    try {
      let defaultPrefs = [];
      try {
          defaultPrefs = this.readDefaultPrefs(FileUtils.getFile("ProfD", [this.DEFAULT_PREFS_FILENAME]));
      } catch(ex) {
          // this can throw if the defaultprefs.js file doesn't exist
      }
      for (let i = 0; i < defaultPrefs.length; i++) {
        this.prefs.push(defaultPrefs[i]);
      }
    } catch(ex) {
      console.log("Error reading defaultPrefs file: " + ex);
    }
    return this.prefs;
  },

  readDefaultPrefs: function webapps_readDefaultPrefs(aFile) {
    let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
    fstream.init(aFile, -1, 0, 0);
    let prefsString = NetUtil.readInputStreamToString(fstream, fstream.available(), {});
    return JSON.parse(prefsString);
  },

  addPref: function(aPref) {
    switch (typeof aPref.value) {
      case "string":
        Services.prefs.setCharPref(aPref.name, aPref.value);
        break;
      case "boolean":
        Services.prefs.setBoolPref(aPref.name, aPref.value);
        break;
      case "number":
        Services.prefs.setIntPref(aPref.name, aPref.value);
        break;
    }
  },

  handleEvent: function(event) {
    let target = event.target;
  
    if (!(target instanceof HTMLAnchorElement) ||
        target.getAttribute("target") != "_blank") {
      return;
    }
  
    let uri = Services.io.newURI(target.href,
                                 target.ownerDocument.characterSet,
                                 null);
  
    // Direct the URL to the browser.
    Cc["@mozilla.org/uriloader/external-protocol-service;1"].
      getService(Ci.nsIExternalProtocolService).
      getProtocolHandlerInfo(uri.scheme).
      launchWithURI(uri);
  
    // Prevent the runtime from loading the URL.  We do this after directing it
    // to the browser to give the runtime a shot at handling the URL if we fail
    // to direct it to the browser for some reason.
    event.preventDefault();
  }
}
