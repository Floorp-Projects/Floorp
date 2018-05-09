"use strict";

const TOOLKIT_ID = "toolkit@mozilla.org";

// We don't have an easy way to serve update manifests from a secure URL.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

var testserver = createHttpServer();
gPort = testserver.identity.primaryPort;

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

const extensionsDir = gProfD.clone();
extensionsDir.append("extensions");

const addonsDir = gTmpD.clone();
addonsDir.append("addons");
addonsDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

registerCleanupFunction(() => addonsDir.remove(true));

testserver.registerDirectory("/addons/", addonsDir);


let gUpdateManifests = {};

function mapManifest(aPath, aManifestData) {
  gUpdateManifests[aPath] = aManifestData;
  testserver.registerPathHandler(aPath, serveManifest);
}

function serveManifest(request, response) {
  let manifest = gUpdateManifests[request.path];

  response.setHeader("Content-Type", manifest.contentType, false);
  response.write(manifest.data);
}

function promiseInstallWebExtension(aData) {
  let addonFile = createTempWebExtensionFile(aData);

  let startupPromise = promiseWebExtensionStartup();

  return promiseInstallAllFiles([addonFile]).then(() => {
    return startupPromise;
  }).then(() => {
    Services.obs.notifyObservers(addonFile, "flush-cache-entry");
    return promiseAddonByID(aData.id);
  });
}

var checkUpdates = async function(aData, aReason = AddonManager.UPDATE_WHEN_PERIODIC_UPDATE) {
  function provide(obj, path, value) {
    path = path.split(".");
    let prop = path.pop();

    for (let key of path) {
      if (!(key in obj))
          obj[key] = {};
      obj = obj[key];
    }

    if (!(prop in obj))
      obj[prop] = value;
  }

  let id = uuidGenerator.generateUUID().number;
  provide(aData, "addon.id", id);
  provide(aData, "addon.manifest.applications.gecko.id", id);

  let updatePath = `/updates/${id}.json`.replace(/[{}]/g, "");
  let updateUrl = `http://localhost:${gPort}${updatePath}`;

  let addonData = { updates: [] };
  let manifestJSON = {
    addons: { [id]: addonData }
  };


  provide(aData, "addon.manifest.applications.gecko.update_url", updateUrl);
  let awaitInstall = promiseInstallWebExtension(aData.addon);


  for (let version of Object.keys(aData.updates)) {
    let update = aData.updates[version];
    update.version = version;

    provide(update, "addon.id", id);
    provide(update, "addon.manifest.applications.gecko.id", id);
    let addon = update.addon;

    delete update.addon;

    let file;
    if (addon.rdf) {
      provide(addon, "version", version);
      provide(addon, "targetApplications", [{id: TOOLKIT_ID,
                                             minVersion: "42",
                                             maxVersion: "*"}]);

      file = createTempXPIFile(addon);
    } else {
      provide(addon, "manifest.version", version);
      file = createTempWebExtensionFile(addon);
    }
    file.moveTo(addonsDir, `${id}-${version}.xpi`.replace(/[{}]/g, ""));

    let path = `/addons/${file.leafName}`;
    provide(update, "update_link", `http://localhost:${gPort}${path}`);

    addonData.updates.push(update);
  }

  mapManifest(updatePath, { data: JSON.stringify(manifestJSON),
                            contentType: "application/json" });


  let addon = await awaitInstall;

  let updates = await promiseFindAddonUpdates(addon, aReason);
  updates.addon = addon;

  return updates;
};


add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42.0", "42.0");

  await promiseStartupManager();
  registerCleanupFunction(promiseShutdownManager);
});


// Check that compatibility updates are applied.
add_task(async function checkUpdateMetadata() {
  let update = await checkUpdates({
    addon: {
      manifest: {
        version: "1.0",
        applications: { gecko: { strict_max_version: "45" } },
      }
    },
    updates: {
      "1.0": {
        applications: { gecko: { strict_min_version: "40",
                                 strict_max_version: "48" } },
      }
    }
  });

  ok(update.compatibilityUpdate, "have compat update");
  ok(!update.updateAvailable, "have no add-on update");

  ok(update.addon.isCompatibleWith("40", "40"), "compatible min");
  ok(update.addon.isCompatibleWith("48", "48"), "compatible max");
  ok(!update.addon.isCompatibleWith("49", "49"), "not compatible max");

  update.addon.uninstall();
});


// Check that updates from web extensions to web extensions succeed.
add_task(async function checkUpdateToWebExt() {
  let update = await checkUpdates({
    addon: { manifest: { version: "1.0" } },
    updates: {
      "1.1": { },
      "1.2": { },
      "1.3": { "applications": { "gecko": { "strict_min_version": "48" } } },
    }
  });

  ok(!update.compatibilityUpdate, "have no compat update");
  ok(update.updateAvailable, "have add-on update");

  equal(update.addon.version, "1.0", "add-on version");

  await Promise.all([
    promiseCompleteAllInstalls([update.updateAvailable]),
    promiseWebExtensionStartup(),
  ]);

  let addon = await promiseAddonByID(update.addon.id);
  equal(addon.version, "1.2", "new add-on version");

  addon.uninstall();
});


// Check that updates from web extensions to XUL extensions fail.
add_task(async function checkUpdateToRDF() {
  let update = await checkUpdates({
    addon: { manifest: { version: "1.0" } },
    updates: {
      "1.1": { addon: { rdf: true, bootstrap: true } },
    }
  });

  ok(!update.compatibilityUpdate, "have no compat update");
  ok(update.updateAvailable, "have add-on update");

  equal(update.addon.version, "1.0", "add-on version");

  let result = await new Promise((resolve, reject) => {
    update.updateAvailable.addListener({
      onDownloadFailed: resolve,
      onDownloadEnded: reject,
      onInstalling: reject,
      onInstallStarted: reject,
      onInstallEnded: reject,
    });
    update.updateAvailable.install();
  });

  equal(result.error, AddonManager.ERROR_UNEXPECTED_ADDON_TYPE, "error: unexpected add-on type");

  let addon = await promiseAddonByID(update.addon.id);
  equal(addon.version, "1.0", "new add-on version");

  addon.uninstall();
});


// Check that illegal update URLs are rejected.
add_task(async function checkIllegalUpdateURL() {
  const URLS = ["chrome://browser/content/",
                "data:text/json,...",
                "javascript:;",
                "/"];

  for (let url of URLS) {
    let { messages } = await promiseConsoleOutput(() => {
      let addonFile = createTempWebExtensionFile({
        manifest: { applications: { gecko: { update_url: url } } },
      });

      return AddonManager.getInstallForFile(addonFile).then(install => {
        Services.obs.notifyObservers(addonFile, "flush-cache-entry");

        if (!install || install.state != AddonManager.STATE_DOWNLOAD_FAILED)
          throw new Error("Unexpected state: " + (install && install.state));
      });
    });

    ok(messages.some(msg => /Access denied for URL|may not load or link to|is not a valid URL/.test(msg)),
       "Got checkLoadURI error");
  }
});
