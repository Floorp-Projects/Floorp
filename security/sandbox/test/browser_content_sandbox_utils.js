/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
  Ci.nsIUUIDGenerator
);
const environment = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

/*
 * Utility functions for the browser content sandbox tests.
 */

function sanityChecks() {
  // This test is only relevant in e10s
  if (!gMultiProcessBrowser) {
    ok(false, "e10s is enabled");
    info("e10s is not enabled, exiting");
    return;
  }

  let level = 0;
  let prefExists = true;

  // Read the security.sandbox.content.level pref.
  // eslint-disable-next-line mozilla/use-default-preference-values
  try {
    level = Services.prefs.getIntPref("security.sandbox.content.level");
  } catch (e) {
    prefExists = false;
  }

  ok(prefExists, "pref security.sandbox.content.level exists");
  if (!prefExists) {
    return;
  }

  info(`security.sandbox.content.level=${level}`);
  ok(level > 0, "content sandbox is enabled.");

  let isFileIOSandboxed = isContentFileIOSandboxed(level);

  // Content sandbox enabled, but level doesn't include file I/O sandboxing.
  ok(isFileIOSandboxed, "content file I/O sandboxing is enabled.");
  if (!isFileIOSandboxed) {
    info("content sandbox level too low for file I/O tests, exiting\n");
  }
}

// Creates file at |path| and returns a promise that resolves with an object
// with .ok boolean to indicate true if the file was successfully created,
// otherwise false. Include imports so this can be safely serialized and run
// remotely by ContentTask.spawn.
function createFile(path) {
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  let encoder = new TextEncoder();
  let array = encoder.encode("TEST FILE DUMMY DATA");
  return OS.File.writeAtomic(path, array).then(
    function(value) {
      return { ok: true };
    },
    function(reason) {
      return { ok: false };
    }
  );
}

// Creates a symlink at |path| and returns a promise that resolves with an
// object with .ok boolean to indicate true if the symlink was successfully
// created, otherwise false. Include imports so this can be safely serialized
// and run remotely by ContentTask.spawn.
function createSymlink(path) {
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  // source location for the symlink can be anything
  return OS.File.unixSymLink("/Users", path).then(
    function(value) {
      return { ok: true };
    },
    function(reason) {
      return { ok: false };
    }
  );
}

// Deletes file at |path| and returns a promise that resolves with an object
// with .ok boolean to indicate true if the file was successfully deleted,
// otherwise false. Include imports so this can be safely serialized and run
// remotely by ContentTask.spawn.
function deleteFile(path) {
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  return OS.File.remove(path, { ignoreAbsent: false })
    .then(function(value) {
      return { ok: true };
    })
    .catch(function(err) {
      return { ok: false };
    });
}

// Reads the directory at |path| and returns a promise that resolves when
// iteration over the directory finishes or encounters an error. The promise
// resolves with an object where .ok indicates success or failure and
// .numEntries is the number of directory entries found.
function readDir(path) {
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  let numEntries = 0;
  let iterator = new OS.File.DirectoryIterator(path);
  let promise = iterator
    .forEach(function(dirEntry) {
      numEntries++;
    })
    .then(function() {
      iterator.close();
      return { ok: true, numEntries };
    })
    .catch(function() {
      return { ok: false, numEntries };
    });
  return promise;
}

// Reads the file at |path| and returns a promise that resolves when
// reading is completed. Returned object has boolean .ok to indicate
// success or failure.
function readFile(path) {
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  let promise = OS.File.read(path)
    .then(function(binaryData) {
      return { ok: true };
    })
    .catch(function(error) {
      return { ok: false };
    });
  return promise;
}

// Does a stat of |path| and returns a promise that resolves if the
// stat is successful. Returned object has boolean .ok to indicate
// success or failure.
function statPath(path) {
  const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
  let promise = OS.File.stat(path)
    .then(function(stat) {
      return { ok: true };
    })
    .catch(function(error) {
      return { ok: false };
    });
  return promise;
}

// Returns true if the current content sandbox level, passed in
// the |level| argument, supports filesystem sandboxing.
function isContentFileIOSandboxed(level) {
  let fileIOSandboxMinLevel = 0;

  // Set fileIOSandboxMinLevel to the lowest level that has
  // content filesystem sandboxing enabled. For now, this
  // varies across Windows, Mac, Linux, other.
  switch (Services.appinfo.OS) {
    case "WINNT":
      fileIOSandboxMinLevel = 1;
      break;
    case "Darwin":
      fileIOSandboxMinLevel = 1;
      break;
    case "Linux":
      fileIOSandboxMinLevel = 2;
      break;
    default:
      Assert.ok(false, "Unknown OS");
  }

  return level >= fileIOSandboxMinLevel;
}

// Returns the lowest sandbox level where blanket reading of the profile
// directory from the content process should be blocked by the sandbox.
function minProfileReadSandboxLevel(level) {
  switch (Services.appinfo.OS) {
    case "WINNT":
      return 3;
    case "Darwin":
      return 2;
    case "Linux":
      return 3;
    default:
      Assert.ok(false, "Unknown OS");
      return 0;
  }
}

// Returns the lowest sandbox level where blanket reading of the home
// directory from the content process should be blocked by the sandbox.
function minHomeReadSandboxLevel(level) {
  switch (Services.appinfo.OS) {
    case "WINNT":
      return 3;
    case "Darwin":
      return 3;
    case "Linux":
      return 3;
    default:
      Assert.ok(false, "Unknown OS");
      return 0;
  }
}

function isMac() {
  return Services.appinfo.OS == "Darwin";
}
function isWin() {
  return Services.appinfo.OS == "WINNT";
}
function isLinux() {
  return Services.appinfo.OS == "Linux";
}

function isNightly() {
  let version = SpecialPowers.Services.appinfo.version;
  return version.endsWith("a1");
}

function uuid() {
  return uuidGenerator.generateUUID().toString();
}

// Returns a file object for a new file in the home dir ($HOME/<UUID>).
function fileInHomeDir() {
  // get home directory, make sure it exists
  let homeDir = Services.dirsvc.get("Home", Ci.nsIFile);
  Assert.ok(homeDir.exists(), "Home dir exists");
  Assert.ok(homeDir.isDirectory(), "Home dir is a directory");

  // build a file object for a new file named $HOME/<UUID>
  let homeFile = homeDir.clone();
  homeFile.appendRelativePath(uuid());
  Assert.ok(!homeFile.exists(), homeFile.path + " does not exist");
  return homeFile;
}

// Returns a file object for a new file in the content temp dir (.../<UUID>).
function fileInTempDir() {
  let contentTempKey = "ContentTmpD";

  // get the content temp dir, make sure it exists
  let ctmp = Services.dirsvc.get(contentTempKey, Ci.nsIFile);
  Assert.ok(ctmp.exists(), "Content temp dir exists");
  Assert.ok(ctmp.isDirectory(), "Content temp dir is a directory");

  // build a file object for a new file in content temp
  let tempFile = ctmp.clone();
  tempFile.appendRelativePath(uuid());
  Assert.ok(!tempFile.exists(), tempFile.path + " does not exist");
  return tempFile;
}

function GetProfileDir() {
  // get profile directory
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  return profileDir;
}

function GetHomeDir() {
  // get home directory
  let homeDir = Services.dirsvc.get("Home", Ci.nsIFile);
  return homeDir;
}

function GetHomeSubdir(subdir) {
  return GetSubdir(GetHomeDir(), subdir);
}

function GetHomeSubdirFile(subdir) {
  return GetSubdirFile(GetHomeSubdir(subdir));
}

function GetSubdir(dir, subdir) {
  let newSubdir = dir.clone();
  newSubdir.appendRelativePath(subdir);
  return newSubdir;
}

function GetSubdirFile(dir) {
  let newFile = dir.clone();
  newFile.appendRelativePath(uuid());
  return newFile;
}

function GetPerUserExtensionDir() {
  return Services.dirsvc.get("XREUSysExt", Ci.nsIFile);
}

// Returns a file object for the file or directory named |name| in the
// profile directory.
function GetProfileEntry(name) {
  let entry = GetProfileDir();
  entry.append(name);
  return entry;
}

function GetDir(path) {
  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  dir.initWithPath(path);
  Assert.ok(dir.isDirectory(), `${path} is a directory`);
  return dir;
}

function GetDirFromEnvVariable(varName) {
  return GetDir(environment.get(varName));
}

function GetFile(path) {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);
  return file;
}

function GetEnvironmentVariable(varName) {
  return environment.get(varName);
}

function GetBrowserType(type) {
  let browserType = undefined;

  if (!GetBrowserType[type]) {
    if (type === "web") {
      GetBrowserType[type] = gBrowser.selectedBrowser;
    } else {
      // open a tab in a `type` content process
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
        preferredRemoteType: type,
      });
      // get the browser for the `type` process tab
      GetBrowserType[type] = gBrowser.getBrowserForTab(gBrowser.selectedTab);
    }
  }

  browserType = GetBrowserType[type];
  ok(
    browserType.remoteType === type,
    `GetBrowserType(${type}) returns a ${type} process`
  );
  return browserType;
}

function GetWebBrowser() {
  return GetBrowserType("web");
}

function isFileContentProcessEnabled() {
  // Ensure that the file content process is enabled.
  let fileContentProcessEnabled = Services.prefs.getBoolPref(
    "browser.tabs.remote.separateFileUriProcess"
  );
  ok(fileContentProcessEnabled, "separate file content process is enabled");
  return fileContentProcessEnabled;
}

function GetFileBrowser() {
  if (!isFileContentProcessEnabled()) {
    return undefined;
  }
  return GetBrowserType("file");
}

function GetSandboxLevel() {
  // Current level
  return Services.prefs.getIntPref("security.sandbox.content.level");
}

async function runTestsList(tests) {
  let level = GetSandboxLevel();

  // remove tests not enabled by the current sandbox level
  tests = tests.filter(test => test.minLevel <= level);

  for (let test of tests) {
    let okString = test.ok ? "allowed" : "blocked";
    let processType = test.browser.remoteType;

    // ensure the file/dir exists before we ask a content process to stat
    // it so we know a failure is not due to a nonexistent file/dir
    if (test.func === statPath) {
      ok(test.file.exists(), `${test.file.path} exists`);
    }

    let result = await ContentTask.spawn(
      test.browser,
      test.file.path,
      test.func
    );

    ok(
      result.ok == test.ok,
      `reading ${test.desc} from a ${processType} process ` +
        `is ${okString} (${test.file.path})`
    );

    // if the directory is not expected to be readable,
    // ensure the listing has zero entries
    if (test.func === readDir && !test.ok) {
      ok(result.numEntries == 0, `directory list is empty (${test.file.path})`);
    }

    if (test.cleanup != undefined) {
      await test.cleanup(test.file.path);
    }
  }
}
