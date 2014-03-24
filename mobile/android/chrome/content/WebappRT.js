/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/PermissionsInstaller.jsm");
Cu.import("resource://gre/modules/PermissionPromptHelper.jsm");
Cu.import("resource://gre/modules/ContactService.jsm");
#ifdef MOZ_ANDROID_SYNTHAPKS
Cu.import("resource://gre/modules/AppsUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Notifications", "resource://gre/modules/Notifications.jsm");
#endif

function pref(name, value) {
  return {
    name: name,
    value: value
  }
}

let WebappRT = {
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
    pref("toolkit.telemetry.notifiedOptOut", 999),
    pref("media.useAudioChannelService", true),
    pref("dom.mozTCPSocket.enabled", true),
  ],

  init: function(aStatus, aUrl, aCallback) {
    this.deck = document.getElementById("browsers");
    this.deck.addEventListener("click", this, false, true);

    // on first run, update any prefs
    if (aStatus == "new") {
      this.getDefaultPrefs().forEach(this.addPref);

      // update the blocklist url to use a different app id
      let blocklist = Services.prefs.getCharPref("extensions.blocklist.url");
      blocklist = blocklist.replace(/%APP_ID%/g, "webapprt-mobile@mozilla.org");
      Services.prefs.setCharPref("extensions.blocklist.url", blocklist);
    }

    // On firstrun, set permissions to their default values.
    // When the webapp runtime is updated, update the permissions.
    if (aStatus == "new" || aStatus == "upgrade") {
      this.getManifestFor(aUrl, function (aManifest, aApp) {
        if (aManifest) {
          PermissionsInstaller.installPermissions(aApp, true);
        }
      });
    }

#ifdef MOZ_ANDROID_SYNTHAPKS
    // If the app is in debug mode, configure and enable the remote debugger.
    // sendMessageToJava can only return string values, so it returns the string
    // "true" rather than boolean true if the app is in debug mode.
    if (sendMessageToJava({ type: "NativeApp:IsDebuggable" }) === "true") {
      this._enableRemoteDebugger(aUrl);
    }
#endif

    this.findManifestUrlFor(aUrl, aCallback);
  },

  getManifestFor: function (aUrl, aCallback) {
    DOMApplicationRegistry.registryReady.then(() => {
      let request = navigator.mozApps.mgmt.getAll();
      request.onsuccess = function() {
        let apps = request.result;
        for (let i = 0; i < apps.length; i++) {
          let app = apps[i];
          let manifest = new ManifestHelper(app.manifest, app.origin);

          // if this is a path to the manifest, or the launch path, then we have a hit.
          if (app.manifestURL == aUrl || manifest.fullLaunchPath() == aUrl) {
            aCallback(manifest, app);
            return;
          }
        }

        // Otherwise, once we loop through all of them, we have a miss.
        aCallback(undefined);
      };

      request.onerror = function() {
        // Treat an error like a miss. We can't find the manifest.
        aCallback(undefined);
      };
    });
  },

  findManifestUrlFor: function(aUrl, aCallback) {
    this.getManifestFor(aUrl, function(aManifest, aApp) {
      if (!aManifest) {
        // we can't find the manifest, so open it like a web page
        aCallback(aUrl);
        return;
      }

      BrowserApp.manifest = aManifest;
      BrowserApp.manifestUrl = aApp.manifestURL;

      aCallback(aManifest.fullLaunchPath());
    });
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

#ifdef MOZ_ANDROID_SYNTHAPKS
  _enableRemoteDebugger: function(aUrl) {
    // Skip the connection prompt in favor of notifying the user below.
    Services.prefs.setBoolPref("devtools.debugger.prompt-connection", false);

    // Automagically find a free port and configure the debugger to use it.
    let serv = Cc['@mozilla.org/network/server-socket;1'].createInstance(Ci.nsIServerSocket);
    serv.init(-1, true, -1);
    let port = serv.port;
    serv.close();
    Services.prefs.setIntPref("devtools.debugger.remote-port", port);

    Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);

    // Notify the user that we enabled the debugger and which port it's using
    // so they can use the DevTools Connect… dialog to connect the client to it.
    DOMApplicationRegistry.registryReady.then(() => {
      let name;
      let app = DOMApplicationRegistry.getAppByManifestURL(aUrl);
      if (app) {
        name = app.name;
      } else {
        name = Strings.browser.GetStringFromName("remoteNotificationGenericName");
      }

      Notifications.create({
        title: Strings.browser.formatStringFromName("remoteNotificationTitle", [name], 1),
        message: Strings.browser.formatStringFromName("remoteNotificationMessage", [port], 1),
        icon: "drawable://warning_doorhanger",
      });
    });
  },
#endif

  handleEvent: function(event) {
    let target = event.target;

    // walk up the tree to find the nearest link tag
    while (target && !(target instanceof HTMLAnchorElement)) {
      target = target.parentNode;
    }

    if (!target || target.getAttribute("target") != "_blank") {
      return;
    }

    let uri = Services.io.newURI(target.href, target.ownerDocument.characterSet, null);

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
