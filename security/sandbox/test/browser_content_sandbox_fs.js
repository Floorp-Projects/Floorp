/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
 /* import-globals-from browser_content_sandbox_utils.js */
 "use strict";

var prefs = Cc["@mozilla.org/preferences-service;1"]
            .getService(Ci.nsIPrefBranch);

Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/" +
    "security/sandbox/test/browser_content_sandbox_utils.js", this);

/*
 * This test exercises file I/O from web and file content processes using
 * OS.File methods to validate that calls that are meant to be blocked by
 * content sandboxing are blocked.
 */

// Creates file at |path| and returns a promise that resolves with true
// if the file was successfully created, otherwise false. Include imports
// so this can be safely serialized and run remotely by ContentTask.spawn.
function createFile(path) {
  Components.utils.import("resource://gre/modules/osfile.jsm");
  let encoder = new TextEncoder();
  let array = encoder.encode("WRITING FROM CONTENT PROCESS");
  return OS.File.writeAtomic(path, array).then(function(value) {
    return true;
  }, function(reason) {
    return false;
  });
}

// Deletes file at |path| and returns a promise that resolves with true
// if the file was successfully deleted, otherwise false. Include imports
// so this can be safely serialized and run remotely by ContentTask.spawn.
function deleteFile(path) {
  Components.utils.import("resource://gre/modules/osfile.jsm");
  return OS.File.remove(path, {ignoreAbsent: false}).then(function(value) {
    return true;
  }).catch(function(err) {
    return false;
  });
}

// Reads the directory at |path| and returns a promise that resolves when
// iteration over the directory finishes or encounters an error. The promise
// resolves with an object where .ok indicates success or failure and
// .numEntries is the number of directory entries found.
function readDir(path) {
  Components.utils.import("resource://gre/modules/osfile.jsm");
  let numEntries = 0;
  let iterator = new OS.File.DirectoryIterator(path);
  let promise = iterator.forEach(function (dirEntry) {
    numEntries++;
  }).then(function () {
    iterator.close();
    return {ok: true, numEntries};
  }).catch(function () {
    return {ok: false, numEntries};
  });
  return promise;
}

// Reads the file at |path| and returns a promise that resolves when
// reading is completed. Returned object has boolean .ok to indicate
// success or failure.
function readFile(path) {
  Components.utils.import("resource://gre/modules/osfile.jsm");
  let promise = OS.File.read(path).then(function (binaryData) {
    return {ok: true};
  }).catch(function (error) {
    return {ok: false};
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

  return (level >= fileIOSandboxMinLevel);
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

//
// Checks that sandboxing is enabled and at the appropriate level
// setting before triggering tests that do the file I/O.
//
// Tests attempting to write to a file in the home directory from the
// content process--expected to fail.
//
// Tests attempting to write to a file in the content temp directory
// from the content process--expected to succeed. On Mac and Windows,
// use "ContentTmpD", but on Linux use "TmpD" until Linux uses the
// content temp dir key.
//
// Tests reading various files and directories from file and web
// content processes.
//
add_task(function* () {
  // This test is only relevant in e10s
  if (!gMultiProcessBrowser) {
    ok(false, "e10s is enabled");
    info("e10s is not enabled, exiting");
    return;
  }

  let level = 0;
  let prefExists = true;

  // Read the security.sandbox.content.level pref.
  // If the pref isn't set and we're running on Linux on !isNightly(),
  // exit without failing. The Linux content sandbox is only enabled
  // on Nightly at this time.
  // eslint-disable-next-line mozilla/use-default-preference-values
  try {
    level = prefs.getIntPref("security.sandbox.content.level");
  } catch (e) {
    prefExists = false;
  }

  ok(prefExists, "pref security.sandbox.content.level exists");
  if (!prefExists) {
    return;
  }

  info(`security.sandbox.content.level=${level}`);
  ok(level > 0, "content sandbox is enabled.");
  if (level == 0) {
    info("content sandbox is not enabled, exiting");
    return;
  }

  let isFileIOSandboxed = isContentFileIOSandboxed(level);

  // Content sandbox enabled, but level doesn't include file I/O sandboxing.
  ok(isFileIOSandboxed, "content file I/O sandboxing is enabled.");
  if (!isFileIOSandboxed) {
    info("content sandbox level too low for file I/O tests, exiting\n");
    return;
  }

  // Test creating a file in the home directory from a web content process
  add_task(createFileInHome);

  // Test creating a file content temp from a web content process
  add_task(createTempFile);

  // Test reading files/dirs from web and file content processes
  add_task(testFileAccess);
});

// Test if the content process can create in $HOME, this should fail
function* createFileInHome() {
  let browser = gBrowser.selectedBrowser;
  let homeFile = fileInHomeDir();
  let path = homeFile.path;
  let fileCreated = yield ContentTask.spawn(browser, path, createFile);
  ok(fileCreated == false, "creating a file in home dir is not permitted");
  if (fileCreated == true) {
    // content process successfully created the file, now remove it
    homeFile.remove(false);
  }
}

// Test if the content process can create a temp file, should pass
function* createTempFile() {
  let browser = gBrowser.selectedBrowser;
  let path = fileInTempDir().path;
  let fileCreated = yield ContentTask.spawn(browser, path, createFile);
  ok(fileCreated == true, "creating a file in content temp is permitted");
  // now delete the file
  let fileDeleted = yield ContentTask.spawn(browser, path, deleteFile);
  ok(fileDeleted == true, "deleting a file in content temp is permitted");
}

// Test reading files and dirs from web and file content processes.
function* testFileAccess() {
  // for tests that run in a web content process
  let webBrowser = gBrowser.selectedBrowser;

  // For now, we'll only test file access from the file content process if
  // the file content process is enabled. Once the file content process is
  // ready to ride the trains, this test should be changed to always test
  // file content process access. We use todo() to cause failures
  // if the file process is enabled on a non-Nightly release so that we'll
  // know to update this test to always run the tests. i.e., we'll want to
  // catch bugs that accidentally disable file content process.
  let fileContentProcessEnabled =
    prefs.getBoolPref("browser.tabs.remote.separateFileUriProcess");
  if (isNightly()) {
    ok(fileContentProcessEnabled, "separate file content process is enabled");
  } else {
    todo(fileContentProcessEnabled, "separate file content process is enabled");
  }

  // for tests that run in a file content process
  let fileBrowser = undefined;
  if (fileContentProcessEnabled) {
    // open a tab in a file content process
    gBrowser.selectedTab =
      BrowserTestUtils.addTab(gBrowser, "about:blank", {preferredRemoteType: "file"});
    // get the browser for the file content process tab
    fileBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  }

  // Current level
  let level = prefs.getIntPref("security.sandbox.content.level");

  // Directories/files to test accessing from content processes.
  // For directories, we test whether a directory listing is allowed
  // or blocked. For files, we test if we can read from the file.
  // Each entry in the array represents a test file or directory
  // that will be read from either a web or file process.
  let tests = [];

  let profileDir = GetProfileDir();
  tests.push({
    desc:     "profile dir",                // description
    ok:       false,                        // expected to succeed?
    browser:  webBrowser,                   // browser to run test in
    file:     profileDir,                   // nsIFile object
    minLevel: minProfileReadSandboxLevel(), // min level to enable test
  });
  if (fileContentProcessEnabled) {
    tests.push({
      desc:     "profile dir",
      ok:       true,
      browser:  fileBrowser,
      file:     profileDir,
      minLevel: 0,
    });
  }

  let homeDir = GetHomeDir();
  tests.push({
    desc:     "home dir",
    ok:       false,
    browser:  webBrowser,
    file:     homeDir,
    minLevel: minHomeReadSandboxLevel(),
  });
  if (fileContentProcessEnabled) {
    tests.push({
      desc:     "home dir",
      ok:       true,
      browser:  fileBrowser,
      file:     homeDir,
      minLevel: 0,
    });
  }

  if (isMac()) {
    // If ~/Library/Caches/TemporaryItems exists, when level <= 2 we
    // make sure it's readable. For level 3, we make sure it isn't.
    let homeTempDir = GetHomeDir();
    homeTempDir.appendRelativePath("Library/Caches/TemporaryItems");
    if (homeTempDir.exists()) {
      let shouldBeReadable, minLevel;
      if (level >= minHomeReadSandboxLevel()) {
        shouldBeReadable = false;
        minLevel = minHomeReadSandboxLevel();
      } else {
        shouldBeReadable = true;
        minLevel = 0;
      }
      tests.push({
        desc:     "home library cache temp dir",
        ok:       shouldBeReadable,
        browser:  webBrowser,
        file:     homeTempDir,
        minLevel,
      });
    }
  }

  // Should we enable this /var test on Linux? Once we are running
  // with read access restrictions on Linux, this todo will fail and
  // should then be removed.
  if (isLinux()) {
    todo(level >= minHomeReadSandboxLevel(), "enable /var test on Linux?");
  }
  if (isMac()) {
    let varDir = GetDir("/var");

    // Mac sandbox rules use /private/var because /var is a symlink
    // to /private/var on OS X. Make sure that hasn't changed.
    varDir.normalize();
    Assert.ok(varDir.path === "/private/var", "/var resolves to /private/var");

    tests.push({
      desc:     "/var",
      ok:       false,
      browser:  webBrowser,
      file:     varDir,
      minLevel: minHomeReadSandboxLevel(),
    });
    if (fileContentProcessEnabled) {
      tests.push({
        desc:     "/var",
        ok:       true,
        browser:  fileBrowser,
        file:     varDir,
        minLevel: 0,
      });
    }
  }

  if (isMac()) {
    // Test if we can read from $TMPDIR because we expect it
    // to be within /private/var. Reading from it should be
    // prevented in a 'web' process.
    let macTempDir = GetDirFromEnvVariable("TMPDIR");

    macTempDir.normalize();
    Assert.ok(macTempDir.path.startsWith("/private/var"),
      "$TMPDIR is in /private/var");

    tests.push({
      desc:     `$TMPDIR (${macTempDir.path})`,
      ok:       false,
      browser:  webBrowser,
      file:     macTempDir,
      minLevel: minHomeReadSandboxLevel(),
    });
    if (fileContentProcessEnabled) {
      tests.push({
        desc:     `$TMPDIR (${macTempDir.path})`,
        ok:       true,
        browser:  fileBrowser,
        file:     macTempDir,
        minLevel: 0,
      });
    }
  }

  let extensionsDir = GetProfileEntry("extensions");
  if (extensionsDir.exists() && extensionsDir.isDirectory()) {
    tests.push({
      desc:     "extensions dir",
      ok:       true,
      browser:  webBrowser,
      file:     extensionsDir,
      minLevel: 0,
    });
  } else {
    ok(false, `${extensionsDir.path} is a valid dir`);
  }

  let chromeDir = GetProfileEntry("chrome");
  if (chromeDir.exists() && chromeDir.isDirectory()) {
    tests.push({
      desc:     "chrome dir",
      ok:       true,
      browser:  webBrowser,
      file:     chromeDir,
      minLevel: 0,
    });
  } else {
    ok(false, `${chromeDir.path} is valid dir`);
  }

  let cookiesFile = GetProfileEntry("cookies.sqlite");
  if (cookiesFile.exists() && !cookiesFile.isDirectory()) {
    tests.push({
      desc:     "cookies file",
      ok:       false,
      browser:  webBrowser,
      file:     cookiesFile,
      minLevel: minProfileReadSandboxLevel(),
    });
  } else {
    ok(false, `${cookiesFile.path} is a valid file`);
  }

  // remove tests not enabled by the current sandbox level
  tests = tests.filter((test) => (test.minLevel <= level));

  for (let test of tests) {
    let testFunc = test.file.isDirectory ? readDir : readFile;
    let okString = test.ok ? "allowed" : "blocked";
    let processType = test.browser === webBrowser ? "web" : "file";

    let result = yield ContentTask.spawn(test.browser, test.file.path,
        testFunc);

    ok(result.ok == test.ok,
        `reading ${test.desc} from a ${processType} process ` +
        `is ${okString} (${test.file.path})`);

    // if the directory is not expected to be readable,
    // ensure the listing has zero entries
    if (test.file.isDirectory() && !test.ok) {
      ok(result.numEntries == 0, `directory list is empty (${test.file.path})`);
    }
  }

  if (fileContentProcessEnabled) {
    gBrowser.removeTab(gBrowser.selectedTab);
  }
}
