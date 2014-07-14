/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This module is imported at the startup of an application.  It takes care of
 * permissions installation, application url loading, security settings.  Put
 * stuff here that you want to happen once on startup before the webapp is
 * loaded.  */

this.EXPORTED_SYMBOLS = ["startup"];

const Ci = Components.interfaces;
const Cu = Components.utils;

/* We load here modules that are needed to perform the application startup.
 * We lazily load modules that aren't needed on every startup.
 * We load modules that aren't used here but that need to perform some
 * initialization steps later in the startup function. */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

Cu.import("resource://webapprt/modules/WebappRT.jsm");

// Lazily load these modules because we don't need them at every
// startup, but only during first run or runtime update.

XPCOMUtils.defineLazyModuleGetter(this, "PermissionsInstaller",
  "resource://gre/modules/PermissionsInstaller.jsm");

const PROFILE_DIR = OS.Constants.Path.profileDir;

function isFirstRunOrUpdate() {
  let savedBuildID = null;
  try {
    savedBuildID = Services.prefs.getCharPref("webapprt.buildID");
  } catch (e) {}

  let ourBuildID = Services.appinfo.platformBuildID;

  if (ourBuildID != savedBuildID) {
    Services.prefs.setCharPref("webapprt.buildID", ourBuildID);
    return true;
  }

  return false;
}

function writeFile(aPath, aData) {
  return Task.spawn(function() {
    let data = new TextEncoder().encode(aData);
    yield OS.File.writeAtomic(aPath, data, { tmpPath: aPath + ".tmp" });
  });
}

function createBrandingFiles() {
  return Task.spawn(function() {
    let manifest = WebappRT.localeManifest;
    let name = WebappRT.localeManifest.name;
    let developer = " ";
    if (WebappRT.localeManifest.developer) {
      developer = WebappRT.localeManifest.developer.name;
    }

    let brandDTDContent = '<!ENTITY brandShortName "' + name + '">\n\
  <!ENTITY brandFullName "' + name + '">\n\
  <!ENTITY vendorShortName "' + developer + '">\n\
  <!ENTITY trademarkInfo.part1 " ">';

    yield writeFile(OS.Path.join(PROFILE_DIR, "brand.dtd"), brandDTDContent);

    let brandPropertiesContent = 'brandShortName=' + name + '\n\
  brandFullName=' + name + '\n\
  vendorShortName=' + developer;

    yield writeFile(OS.Path.join(PROFILE_DIR, "brand.properties"),
                    brandPropertiesContent);
  });
}

// Observes all the events needed to actually launch an application.
// It waits for XUL window and webapps registry loading.
this.startup = function(window) {
  return Task.spawn(function () {
    // Observe XUL window loading.
    // For tests, it could be already loaded.
    let deferredWindowLoad = Promise.defer();
    if (window.document && window.document.getElementById("content")) {
      deferredWindowLoad.resolve();
    } else {
      window.addEventListener("DOMContentLoaded", function onLoad() {
        window.removeEventListener("DOMContentLoaded", onLoad, false);
        deferredWindowLoad.resolve();
      });
    }

    let appUpdated = false;
    let updatePending = yield WebappRT.isUpdatePending();
    if (updatePending) {
      appUpdated = yield WebappRT.applyUpdate();
    }

    yield WebappRT.configPromise;

    let appData = WebappRT.config.app;

    // Initialize DOMApplicationRegistry by importing Webapps.jsm.
    Cu.import("resource://gre/modules/Webapps.jsm");
    // Initialize window-independent handling of webapps- notifications.
    Cu.import("resource://webapprt/modules/WebappManager.jsm");

    // Wait for webapps registry loading.
    yield DOMApplicationRegistry.registryStarted;
    // Add the currently running app to the registry.
    yield DOMApplicationRegistry.addInstalledApp(appData, appData.manifest,
                                                 appData.updateManifest);

    let manifestURL = appData.manifestURL;
    if (manifestURL) {
      // On firstrun, set permissions to their default values.
      // When the webapp runtime is updated, update the permissions.
      if (isFirstRunOrUpdate(Services.prefs) || appUpdated) {
        PermissionsInstaller.installPermissions(appData, true);
        yield createBrandingFiles();
      }
    }

    // Branding substitution
    let aliasFile = Components.classes["@mozilla.org/file/local;1"]
                              .createInstance(Ci.nsIFile);
    aliasFile.initWithPath(PROFILE_DIR);

    let aliasURI = Services.io.newFileURI(aliasFile);

    Services.io.getProtocolHandler("resource")
               .QueryInterface(Ci.nsIResProtocolHandler)
               .setSubstitution("webappbranding", aliasURI);

    // Wait for XUL window loading
    yield deferredWindowLoad.promise;

    // Load these modules here because they aren't needed right at startup,
    // but they need to be loaded to perform some initialization steps.
    Cu.import("resource://gre/modules/Payment.jsm");
    Cu.import("resource://gre/modules/AlarmService.jsm");
    Cu.import("resource://webapprt/modules/WebRTCHandler.jsm");

    // Get the <browser> element in the webapp.xul window.
    let appBrowser = window.document.getElementById("content");

    // Set the principal to the correct appID and launch the application.
    appBrowser.docShell.setIsApp(WebappRT.appID);
    appBrowser.setAttribute("src", WebappRT.launchURI);

    if (appData.manifest.fullscreen) {
      appBrowser.addEventListener("load", function onLoad() {
        appBrowser.removeEventListener("load", onLoad, true);
        appBrowser.contentDocument.
          documentElement.mozRequestFullScreen();
      }, true);
    }

    WebappRT.startUpdateService();
  });
}
