/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm", {});
const { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm", {});
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm", {});
const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm", {});

const NS_ERROR_START_PROFILE_MANAGER = 0x805800c9;

let gProfD = do_get_profile();
let gDataHome = gProfD.clone();
gDataHome.append("data");
gDataHome.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
let gDataHomeLocal = gProfD.clone();
gDataHomeLocal.append("local");
gDataHomeLocal.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);

let xreDirProvider = Cc["@mozilla.org/xre/directory-provider;1"].
                     getService(Ci.nsIXREDirProvider);
xreDirProvider.setUserDataDirectory(gDataHome, false);
xreDirProvider.setUserDataDirectory(gDataHomeLocal, true);

function getProfileService() {
  return Cc["@mozilla.org/toolkit/profile-service;1"].
         getService(Ci.nsIToolkitProfileService);
}

let PROFILE_DEFAULT = "default";
if (AppConstants.MOZ_DEV_EDITION) {
  PROFILE_DEFAULT = "dev-edition-default";
}

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
function selectStartupProfile(args = [], isResetting = false) {
  let rootDir = {};
  let localDir = {};
  let profile = {};
  let didCreate = getProfileService().selectStartupProfile(["xpcshell", ...args], isResetting,
                                                           rootDir, localDir, profile);

  if (profile.value) {
    Assert.ok(rootDir.value.equals(profile.value.rootDir), "Should have matched the root dir.");
    Assert.ok(localDir.value.equals(profile.value.localDir), "Should have matched the local dir.");
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
  } catch (e) {
    Assert.equal(e.result, NS_ERROR_START_PROFILE_MANAGER, "Should have started the profile manager");
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
 * Writes a profiles.ini based on the passed profile data.
 * profileData should contain two properties, options and profiles.
 * options contains a single property, startWithLastProfile.
 * profiles is an array of profiles each containing name, path and default
 * properties.
 */
function writeProfilesIni(profileData) {
  let target = gDataHome.clone();
  target.append("profiles.ini");

  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                getService(Ci.nsIINIParserFactory);
  let ini = factory.createINIParser().QueryInterface(Ci.nsIINIParserWriter);

  const { options = {}, profiles = [] } = profileData;

  let { startWithLastProfile = true } = options;
  ini.setString("General", "StartWithLastProfile", startWithLastProfile ? "1" : "0");

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
    options: {},
    profiles: [],
  };

  if (!target.exists()) {
    return profileData;
  }

  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                getService(Ci.nsIINIParserFactory);
  let ini = factory.createINIParser(target);

  profileData.options.startWithLastProfile = safeGet(ini, "General", "StartWithLastProfile") == "1";

  for (let i = 0; true; i++) {
    let section = `Profile${i}`;

    let isRelative = safeGet(ini, section, "IsRelative");
    if (isRelative === null) {
      break;
    }
    Assert.equal(isRelative, "1", "Paths should always be relative in these tests.");

    let profile = {
      name: safeGet(ini, section, "Name"),
      path: safeGet(ini, section, "Path"),
    };

    try {
      profile.default = ini.getString(section, "Default") == "1";
      Assert.ok(profile.default, "The Default value is only written when true.");
    } catch (e) {
      profile.default = false;
    }

    profileData.profiles.push(profile);
  }

  profileData.profiles.sort((a, b) => a.name.localeCompare(b.name));

  return profileData;
}

/**
 * Checks that the profile service seems to have the right data in it compared
 * to profile and install data structured as in the above functions.
 */
function checkProfileService(profileData = readProfilesIni()) {
  let service = getProfileService();

  let serviceProfiles = Array.from(service.profiles);

  Assert.equal(serviceProfiles.length, profileData.profiles.length, "Should be the same number of profiles.");

  // Sort to make matching easy.
  serviceProfiles.sort((a, b) => a.name.localeCompare(b.name));
  profileData.profiles.sort((a, b) => a.name.localeCompare(b.name));

  let defaultProfile = null;

  for (let i = 0; i < serviceProfiles.length; i++) {
    let serviceProfile = serviceProfiles[i];
    let expectedProfile = profileData.profiles[i];

    Assert.equal(serviceProfile.name, expectedProfile.name, "Should have the same name.");

    let expectedPath = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    expectedPath.setRelativeDescriptor(gDataHome, expectedProfile.path);
    Assert.equal(serviceProfile.rootDir.path, expectedPath.path, "Should have the same path.");

    if (AppConstants.MOZ_DEV_EDITION) {
      if (expectedProfile.name == PROFILE_DEFAULT) {
        defaultProfile = serviceProfile;
      }
    } else if (expectedProfile.default) {
      defaultProfile = serviceProfile;
    }
  }

  let selectedProfile = null;
  try {
    selectedProfile = service.selectedProfile;
  } catch (e) {
    // GetSelectedProfile throws when there are no profiles.
  }

  Assert.equal(selectedProfile, defaultProfile, "Should have seen the right profile selected.");
}
