/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const NS_ERROR_START_PROFILE_MANAGER = 0x805800c9;

const UPDATE_CHANNEL = AppConstants.MOZ_UPDATE_CHANNEL;

let gProfD = do_get_profile();
let gDataHome = gProfD.clone();
gDataHome.append("data");
gDataHome.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
let gDataHomeLocal = gProfD.clone();
gDataHomeLocal.append("local");
gDataHomeLocal.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

let xreDirProvider = Cc["@mozilla.org/xre/directory-provider;1"].getService(
  Ci.nsIXREDirProvider
);
xreDirProvider.setUserDataDirectory(gDataHome, false);
xreDirProvider.setUserDataDirectory(gDataHomeLocal, true);

let gIsDefaultApp = false;

const ShellService = {
  register() {
    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

    let factory = {
      createInstance(iid) {
        return ShellService.QueryInterface(iid);
      },
    };

    registrar.registerFactory(
      this.ID,
      "ToolkitShellService",
      this.CONTRACT,
      factory
    );
  },

  isDefaultApplication() {
    return gIsDefaultApp;
  },

  QueryInterface: ChromeUtils.generateQI(["nsIToolkitShellService"]),
  ID: Components.ID("{ce724e0c-ed70-41c9-ab31-1033b0b591be}"),
  CONTRACT: "@mozilla.org/toolkit/shell-service;1",
};

ShellService.register();

let gIsLegacy = false;

function simulateSnapEnvironment() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("SNAP_INSTANCE_NAME", AppConstants.MOZ_APP_NAME);

  gIsLegacy = true;
}

function enableLegacyProfiles() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("MOZ_LEGACY_PROFILES", "1");

  gIsLegacy = true;
}

function getProfileService() {
  return Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );
}

let PROFILE_DEFAULT = "default";
let DEDICATED_NAME = `default-${UPDATE_CHANNEL}`;
if (AppConstants.MOZ_DEV_EDITION) {
  DEDICATED_NAME = PROFILE_DEFAULT = "dev-edition-default";
}

// Shared data for backgroundtasks tests.
const BACKGROUNDTASKS_PROFILE_DATA = (() => {
  let hash = xreDirProvider.getInstallHash();
  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [
      {
        name: "Profile1",
        path: "Path1",
        default: false,
      },
      {
        name: "Profile3",
        path: "Path3",
        default: false,
      },
    ],
    installs: {
      [hash]: {
        default: "Path1",
      },
    },
    backgroundTasksProfiles: [
      {
        name: `MozillaBackgroundTask-${hash}-unrelated_task`,
        path: `saltsalt.MozillaBackgroundTask-${hash}-unrelated_task`,
      },
    ],
  };
  return profileData;
})();

/**
 * Creates a random profile path for use.
 */
function makeRandomProfileDir(name) {
  let file = gDataHome.clone();
  file.append(name);
  file.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  return file;
}

/**
 * A wrapper around nsIToolkitProfileService.selectStartupProfile to make it
 * a bit nicer to use from JS.
 */
function selectStartupProfile(args = [], isResetting = false, legacyHash = "") {
  let service = getProfileService();
  let rootDir = {};
  let localDir = {};
  let profile = {};
  let didCreate = service.selectStartupProfile(
    ["xpcshell", ...args],
    isResetting,
    UPDATE_CHANNEL,
    legacyHash,
    rootDir,
    localDir,
    profile
  );

  if (profile.value) {
    Assert.ok(
      rootDir.value.equals(profile.value.rootDir),
      "Should have matched the root dir."
    );
    Assert.ok(
      localDir.value.equals(profile.value.localDir),
      "Should have matched the local dir."
    );
    Assert.ok(
      service.currentProfile === profile.value,
      "Should have marked the profile as the current profile."
    );
  } else {
    Assert.ok(!service.currentProfile, "Should be no current profile.");
  }

  return {
    rootDir: rootDir.value,
    localDir: localDir.value,
    profile: profile.value,
    didCreate,
  };
}

function testStartsProfileManager(args = [], isResetting = false) {
  try {
    selectStartupProfile(args, isResetting);
    Assert.ok(false, "Should have started the profile manager");
    checkStartupReason();
  } catch (e) {
    Assert.equal(
      e.result,
      NS_ERROR_START_PROFILE_MANAGER,
      "Should have started the profile manager"
    );
  }
}

function safeGet(ini, section, key) {
  try {
    return ini.getString(section, key);
  } catch (e) {
    return null;
  }
}

/**
 * Writes a compatibility.ini file that marks the give profile directory as last
 * used by the given install path.
 */
function writeCompatibilityIni(
  dir,
  appDir = FileUtils.getDir("CurProcD", []),
  greDir = FileUtils.getDir("GreD", [])
) {
  let target = dir.clone();
  target.append("compatibility.ini");

  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].getService(
    Ci.nsIINIParserFactory
  );
  let ini = factory.createINIParser().QueryInterface(Ci.nsIINIParserWriter);

  // The profile service doesn't care about these so just use fixed values
  ini.setString(
    "Compatibility",
    "LastVersion",
    "64.0a1_20180919123806/20180919123806"
  );
  ini.setString("Compatibility", "LastOSABI", "Darwin_x86_64-gcc3");

  ini.setString(
    "Compatibility",
    "LastPlatformDir",
    greDir.persistentDescriptor
  );
  ini.setString("Compatibility", "LastAppDir", appDir.persistentDescriptor);

  ini.writeFile(target);
}

/**
 * Writes a profiles.ini based on the passed profile data.
 * profileData should contain two properties, options and profiles.
 * options contains a single property, startWithLastProfile.
 * profiles is an array of profiles each containing name, path and default
 * properties.
 */
function writeProfilesIni(profileData) {
  let target = gDataHome.clone();
  target.append("profiles.ini");

  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].getService(
    Ci.nsIINIParserFactory
  );
  let ini = factory.createINIParser().QueryInterface(Ci.nsIINIParserWriter);

  const {
    options = {},
    profiles = [],
    installs = null,
    backgroundTasksProfiles = null,
  } = profileData;

  let { startWithLastProfile = true } = options;
  ini.setString(
    "General",
    "StartWithLastProfile",
    startWithLastProfile ? "1" : "0"
  );

  for (let i = 0; i < profiles.length; i++) {
    let profile = profiles[i];
    let section = `Profile${i}`;

    ini.setString(section, "Name", profile.name);
    ini.setString(section, "IsRelative", 1);
    ini.setString(section, "Path", profile.path);

    if (profile.default) {
      ini.setString(section, "Default", "1");
    }
  }

  if (backgroundTasksProfiles) {
    let section = "BackgroundTasksProfiles";
    for (let backgroundTasksProfile of backgroundTasksProfiles) {
      ini.setString(
        section,
        backgroundTasksProfile.name,
        backgroundTasksProfile.path
      );
    }
  }

  if (installs) {
    ini.setString("General", "Version", "2");

    for (let hash of Object.keys(installs)) {
      ini.setString(`Install${hash}`, "Default", installs[hash].default);
      if ("locked" in installs[hash]) {
        ini.setString(
          `Install${hash}`,
          "Locked",
          installs[hash].locked ? "1" : "0"
        );
      }
    }

    writeInstallsIni({ installs });
  } else {
    writeInstallsIni(null);
  }

  ini.writeFile(target);
}

/**
 * Reads the existing profiles.ini into the same structure as that accepted by
 * writeProfilesIni above. The profiles property is sorted according to name
 * because the order is irrelevant and it makes testing easier if we can make
 * that assumption.
 */
function readProfilesIni() {
  let target = gDataHome.clone();
  target.append("profiles.ini");

  let profileData = {
    options: {
      startWithLastProfile: true,
    },
    profiles: [],
    installs: null,
  };

  if (!target.exists()) {
    return profileData;
  }

  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].getService(
    Ci.nsIINIParserFactory
  );
  let ini = factory.createINIParser(target);

  profileData.options.startWithLastProfile =
    safeGet(ini, "General", "StartWithLastProfile") == "1";
  if (safeGet(ini, "General", "Version") == "2") {
    profileData.installs = {};
  }

  let sections = ini.getSections();
  while (sections.hasMore()) {
    let section = sections.getNext();

    if (section == "General") {
      continue;
    }

    if (section.startsWith("Profile")) {
      let isRelative = safeGet(ini, section, "IsRelative");
      if (isRelative === null) {
        break;
      }
      Assert.equal(
        isRelative,
        "1",
        "Paths should always be relative in these tests."
      );

      let profile = {
        name: safeGet(ini, section, "Name"),
        path: safeGet(ini, section, "Path"),
      };

      try {
        profile.default = ini.getString(section, "Default") == "1";
        Assert.ok(
          profile.default,
          "The Default value is only written when true."
        );
      } catch (e) {
        profile.default = false;
      }

      profileData.profiles.push(profile);
    }

    if (section.startsWith("Install")) {
      Assert.ok(
        profileData.installs,
        "Should only see an install section if the ini version was correct."
      );

      profileData.installs[section.substring(7)] = {
        default: safeGet(ini, section, "Default"),
      };

      let locked = safeGet(ini, section, "Locked");
      if (locked !== null) {
        profileData.installs[section.substring(7)].locked = locked;
      }
    }

    if (section == "BackgroundTasksProfiles") {
      profileData.backgroundTasksProfiles = [];
      let backgroundTasksProfiles = ini.getKeys(section);
      while (backgroundTasksProfiles.hasMore()) {
        let name = backgroundTasksProfiles.getNext();
        let path = ini.getString(section, name);
        profileData.backgroundTasksProfiles.push({ name, path });
      }
      profileData.backgroundTasksProfiles.sort((a, b) =>
        a.name.localeCompare(b.name)
      );
    }
  }

  profileData.profiles.sort((a, b) => a.name.localeCompare(b.name));

  return profileData;
}

/**
 * Writes an installs.ini based on the supplied data. Should be an object with
 * keys for every installation hash each mapping to an object. Each object
 * should have a default property for the relative path to the profile.
 */
function writeInstallsIni(installData) {
  let target = gDataHome.clone();
  target.append("installs.ini");

  if (!installData) {
    try {
      target.remove(false);
    } catch (e) {}
    return;
  }

  const { installs = {} } = installData;

  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].getService(
    Ci.nsIINIParserFactory
  );
  let ini = factory.createINIParser(null).QueryInterface(Ci.nsIINIParserWriter);

  for (let hash of Object.keys(installs)) {
    ini.setString(hash, "Default", installs[hash].default);
    if ("locked" in installs[hash]) {
      ini.setString(hash, "Locked", installs[hash].locked ? "1" : "0");
    }
  }

  ini.writeFile(target);
}

/**
 * Reads installs.ini into a structure like that used in the above function.
 */
function readInstallsIni() {
  let target = gDataHome.clone();
  target.append("installs.ini");

  let installData = {
    installs: {},
  };

  if (!target.exists()) {
    return installData;
  }

  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].getService(
    Ci.nsIINIParserFactory
  );
  let ini = factory.createINIParser(target);

  let sections = ini.getSections();
  while (sections.hasMore()) {
    let hash = sections.getNext();
    if (hash != "General") {
      installData.installs[hash] = {
        default: safeGet(ini, hash, "Default"),
      };

      let locked = safeGet(ini, hash, "Locked");
      if (locked !== null) {
        installData.installs[hash].locked = locked;
      }
    }
  }

  return installData;
}

/**
 * Check that the backup data in installs.ini matches the install data in
 * profiles.ini.
 */
function checkBackup(
  profileData = readProfilesIni(),
  installData = readInstallsIni()
) {
  if (!profileData.installs) {
    // If the profiles db isn't of the right version we wouldn't expect the
    // backup to be accurate.
    return;
  }

  Assert.deepEqual(
    profileData.installs,
    installData.installs,
    "Backup installs.ini should match installs in profiles.ini"
  );
}

/**
 * Checks that the profile service seems to have the right data in it compared
 * to profile and install data structured as in the above functions.
 */
function checkProfileService(
  profileData = readProfilesIni(),
  verifyBackup = true
) {
  let service = getProfileService();

  let expectedStartWithLast = true;
  if ("options" in profileData) {
    expectedStartWithLast = profileData.options.startWithLastProfile;
  }

  Assert.equal(
    service.startWithLastProfile,
    expectedStartWithLast,
    "Start with last profile should match."
  );

  let serviceProfiles = Array.from(service.profiles);

  Assert.equal(
    serviceProfiles.length,
    profileData.profiles.length,
    "Should be the same number of profiles."
  );

  // Sort to make matching easy.
  serviceProfiles.sort((a, b) => a.name.localeCompare(b.name));
  profileData.profiles.sort((a, b) => a.name.localeCompare(b.name));

  let hash = xreDirProvider.getInstallHash();
  let defaultPath =
    profileData.installs && hash in profileData.installs
      ? profileData.installs[hash].default
      : null;
  let dedicatedProfile = null;
  let legacyProfile = null;

  for (let i = 0; i < serviceProfiles.length; i++) {
    let serviceProfile = serviceProfiles[i];
    let expectedProfile = profileData.profiles[i];

    Assert.equal(
      serviceProfile.name,
      expectedProfile.name,
      "Should have the same name."
    );

    let expectedPath = Cc["@mozilla.org/file/local;1"].createInstance(
      Ci.nsIFile
    );
    expectedPath.setRelativeDescriptor(gDataHome, expectedProfile.path);
    Assert.equal(
      serviceProfile.rootDir.path,
      expectedPath.path,
      "Should have the same path."
    );

    if (expectedProfile.path == defaultPath) {
      dedicatedProfile = serviceProfile;
    }

    if (AppConstants.MOZ_DEV_EDITION) {
      if (expectedProfile.name == PROFILE_DEFAULT) {
        legacyProfile = serviceProfile;
      }
    } else if (expectedProfile.default) {
      legacyProfile = serviceProfile;
    }
  }

  if (gIsLegacy) {
    Assert.equal(
      service.defaultProfile,
      legacyProfile,
      "Should have seen the right profile selected."
    );
  } else {
    Assert.equal(
      service.defaultProfile,
      dedicatedProfile,
      "Should have seen the right profile selected."
    );
  }

  if (verifyBackup) {
    checkBackup(profileData);
  }
}

function checkStartupReason(expected = undefined) {
  const tId = "startup.profile_selection_reason";
  let scalars = TelemetryTestUtils.getProcessScalars("parent");

  if (expected === undefined) {
    Assert.ok(
      !(tId in scalars),
      "Startup telemetry should not have been recorded."
    );
    return;
  }

  if (tId in scalars) {
    Assert.equal(
      scalars[tId],
      expected,
      "Should have seen the right startup reason."
    );
  } else {
    Assert.ok(false, "Startup telemetry should have been recorded.");
  }
}
