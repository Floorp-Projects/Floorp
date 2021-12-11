"use strict";

// Globals
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "CloudStorage",
  "resource://gre/modules/CloudStorage.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);

const CLOUD_SERVICES_PREF = "cloud.services.";
const DROPBOX_DOWNLOAD_FOLDER = "Dropbox";
const GOOGLE_DRIVE_DOWNLOAD_FOLDER = "Google Drive";
const DROPBOX_CONFIG_FOLDER =
  AppConstants.platform === "win" ? "Dropbox" : ".dropbox";
const DROPBOX_KEY = "Dropbox";
const GDRIVE_KEY = "GDrive";

var nsIDropboxFile, nsIGDriveFile;

function run_test() {
  initPrefs();
  registerFakePath("Home", do_get_file("cloud/"));
  registerFakePath("LocalAppData", do_get_file("cloud/"));
  registerCleanupFunction(() => {
    cleanupPrefs();
  });
  run_next_test();
}

function initPrefs() {
  Services.prefs.setBoolPref(CLOUD_SERVICES_PREF + "api.enabled", true);
}

/**
 * Replaces a directory service entry with a given nsIFile.
 */
function registerFakePath(key, file) {
  let dirsvc = Services.dirsvc.QueryInterface(Ci.nsIProperties);
  let originalFile;
  try {
    // If a file is already provided save it and undefine, otherwise set will
    // throw for persistent entries (ones that are cached).
    originalFile = dirsvc.get(key, Ci.nsIFile);
    dirsvc.undefine(key);
  } catch (e) {
    // dirsvc.get will throw if nothing provides for the key and dirsvc.undefine
    // will throw if it's not a persistent entry, in either case we don't want
    // to set the original file in cleanup.
    originalFile = undefined;
  }

  dirsvc.set(key, file);
  registerCleanupFunction(() => {
    dirsvc.undefine(key);
    if (originalFile) {
      dirsvc.set(key, originalFile);
    }
  });
}

function mock_dropbox() {
  let discoveryFolder = null;
  if (AppConstants.platform === "win") {
    discoveryFolder = FileUtils.getFile("LocalAppData", [
      DROPBOX_CONFIG_FOLDER,
    ]);
  } else {
    discoveryFolder = FileUtils.getFile("Home", [DROPBOX_CONFIG_FOLDER]);
  }
  discoveryFolder.append("info.json");
  let fileDir = discoveryFolder.parent;
  if (!fileDir.exists()) {
    fileDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }
  do_get_file("cloud/info.json").copyTo(fileDir, "info.json");
  let exist = fileDir.exists();
  Assert.ok(exist, "file exists on desktop");

  // Mock Dropbox Download folder in Home directory
  let downloadFolder = FileUtils.getFile("Home", [
    DROPBOX_DOWNLOAD_FOLDER,
    "Downloads",
  ]);
  if (!downloadFolder.exists()) {
    downloadFolder.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }

  registerCleanupFunction(() => {
    if (discoveryFolder.exists()) {
      discoveryFolder.remove(false);
    }
    if (downloadFolder.exists()) {
      downloadFolder.remove(false);
    }
  });

  return discoveryFolder;
}

function mock_gdrive() {
  let discoveryFolder = null;
  if (AppConstants.platform === "win") {
    discoveryFolder = FileUtils.getFile("LocalAppData", ["Google", "Drive"]);
  } else {
    discoveryFolder = FileUtils.getFile("Home", [
      "Library",
      "Application Support",
      "Google",
      "Drive",
    ]);
  }
  if (!discoveryFolder.exists()) {
    discoveryFolder.createUnique(
      Ci.nsIFile.DIRECTORY_TYPE,
      FileUtils.PERMS_DIRECTORY
    );
  }
  let exist = discoveryFolder.exists();
  Assert.ok(exist, "file exists on desktop");

  // Mock Google Drive Download folder in Home directory
  let downloadFolder = FileUtils.getFile("Home", [
    GOOGLE_DRIVE_DOWNLOAD_FOLDER,
    "Downloads",
  ]);
  if (!downloadFolder.exists()) {
    downloadFolder.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }

  registerCleanupFunction(() => {
    if (discoveryFolder.exists()) {
      discoveryFolder.remove(false);
    }
    if (downloadFolder.exists()) {
      downloadFolder.remove(false);
    }
  });

  return discoveryFolder;
}

function cleanupPrefs() {
  try {
    Services.prefs.clearUserPref(CLOUD_SERVICES_PREF + "lastprompt");
    Services.prefs.clearUserPref(CLOUD_SERVICES_PREF + "storage.key");
    Services.prefs.clearUserPref(CLOUD_SERVICES_PREF + "rejected.key");
    Services.prefs.clearUserPref(CLOUD_SERVICES_PREF + "interval.prompt");
    Services.prefs.clearUserPref(CLOUD_SERVICES_PREF + "api.enabled");
    Services.prefs.setIntPref("browser.download.folderList", 2);
  } catch (e) {
    do_throw("Failed to cleanup prefs: " + e);
  }
}

function promiseGetStorageProviders() {
  return CloudStorage.getStorageProviders();
}

function promisePromptInfo() {
  return CloudStorage.promisePromptInfo();
}

async function checkScan(expectedKey) {
  let metadata = await promiseGetStorageProviders();
  let scanProvider = await promisePromptInfo();

  if (!expectedKey) {
    Assert.equal(metadata.size, 0, "Number of storage providers");
    Assert.ok(!scanProvider, "No provider in scan results");
  } else {
    Assert.ok(metadata.size, "Number of storage providers");
    Assert.equal(
      scanProvider.key,
      expectedKey,
      "Scanned provider key returned"
    );
  }
  return metadata;
}

async function checkSavedPromptResponse(
  aKey,
  metadata,
  remember,
  selected = false
) {
  CloudStorage.savePromptResponse(aKey, remember, selected);

  if (remember && selected) {
    // Save prompt response with option to always remember the setting
    // and provider with aKey selected as cloud storage provider
    // Sets user download settings to always save to cloud

    // Check preferred provider key, should be set to dropbox
    let prefProvider = CloudStorage.getPreferredProvider();
    Assert.equal(
      prefProvider,
      aKey,
      "Saved Response preferred provider key returned"
    );
    // Check browser.download.folderlist pref should be set to 3
    Assert.equal(
      Services.prefs.getIntPref("browser.download.folderList"),
      3,
      "Default download location set to 3"
    );

    // Preferred download folder should be set to provider downloadPath from metadata
    let path = await CloudStorage.getDownloadFolder();
    let nsIDownloadFolder = new FileUtils.File(path);
    Assert.ok(nsIDownloadFolder, "Download folder retrieved");
    Assert.equal(
      nsIDownloadFolder.parent.path,
      metadata.get(aKey).downloadPath,
      "Default download Folder Path"
    );
  } else if (remember && !selected) {
    // Save prompt response with option to always remember the setting
    // and provider with aKey rejected as cloud storage provider
    // Sets cloud.services.rejected.key pref with provider key.
    // Provider is ignored in next scan and never re-prompted again

    let scanResult = await promisePromptInfo();
    if (scanResult) {
      Assert.notEqual(
        scanResult.key,
        DROPBOX_KEY,
        "Scanned provider key returned is not Dropbox"
      );
    } else {
      Assert.ok(!scanResult, "No provider in scan results");
    }
  }
}

add_task(async function test_checkInit() {
  let { CloudStorageInternal } = ChromeUtils.import(
    "resource://gre/modules/CloudStorage.jsm",
    null
  );
  let isInitialized = await CloudStorageInternal.promiseInit;
  Assert.ok(isInitialized, "Providers Metadata successfully initialized");
});

add_task(async function test_noStorageProvider() {
  await checkScan();
  cleanupPrefs();
});

/**
 * Check scan and save prompt response flow if only dropbox exists on desktop.
 */
add_task(async function test_dropboxStorageProvider() {
  nsIDropboxFile = mock_dropbox();
  let result = await checkScan(DROPBOX_KEY);

  // Always save to cloud
  await checkSavedPromptResponse(DROPBOX_KEY, result, true, true);
  cleanupPrefs();

  // Reject dropbox as cloud storage provider and never re-prompt again
  await checkSavedPromptResponse(DROPBOX_KEY, result, true);

  // Uninstall dropbox by removing discovery folder
  nsIDropboxFile.remove(false);
  cleanupPrefs();
});

/**
 * Check scan and save prompt response flow if only gdrive exists on desktop.
 */
add_task(async function test_gDriveStorageProvider() {
  nsIGDriveFile = mock_gdrive();
  let result;
  if (AppConstants.platform === "linux") {
    result = await checkScan();
  } else {
    result = await checkScan(GDRIVE_KEY);
  }

  if (result.size || AppConstants.platform !== "linux") {
    // Always save to cloud
    await checkSavedPromptResponse(GDRIVE_KEY, result, true, true);
    cleanupPrefs();

    // Reject Google Drive as cloud storage provider and never re-prompt again
    await checkSavedPromptResponse(GDRIVE_KEY, result, true);
  }
  // Uninstall gDrive by removing  discovery folder /Home/Library/Application Support/Google/Drive
  nsIGDriveFile.remove(false);
  cleanupPrefs();
});

/**
 * Check scan and save prompt response flow if multiple provider exists on desktop.
 */
add_task(async function test_multipleStorageProvider() {
  nsIDropboxFile = mock_dropbox();
  nsIGDriveFile = mock_gdrive();

  // Dropbox picked by scan if multiple providers found
  let result = await checkScan(DROPBOX_KEY);

  // Always save to cloud
  await checkSavedPromptResponse(DROPBOX_KEY, result, true, true);
  cleanupPrefs();

  // Reject dropbox as cloud storage provider and never re-prompt again
  await checkSavedPromptResponse(DROPBOX_KEY, result, true);

  // Uninstall dropbox and gdrive by removing discovery folder
  nsIDropboxFile.remove(false);
  nsIGDriveFile.remove(false);
  cleanupPrefs();
});
