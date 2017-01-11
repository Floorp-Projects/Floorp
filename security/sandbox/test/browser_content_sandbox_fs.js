/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var prefs = Cc["@mozilla.org/preferences-service;1"]
            .getService(Ci.nsIPrefBranch);

Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/" +
    "security/sandbox/test/browser_content_sandbox_utils.js", this);

/*
 * This test exercises file I/O from the content process using OS.File
 * methods to validate that calls that are meant to be blocked by content
 * sandboxing are blocked.
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

//
// Drive tests for a single content process.
//
// Tests attempting to write to a file in the home directory from the
// content process--expected to fail.
//
// Tests attempting to write to a file in the content temp directory
// from the content process--expected to succeed. On Mac and Windows,
// use "ContentTmpD", but on Linux use "TmpD" until Linux uses the
// content temp dir key.
//
add_task(function*() {
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
  try {
    level = prefs.getIntPref("security.sandbox.content.level");
  } catch (e) {
    prefExists = false;
  }

  // Special case Linux on !isNightly
  if (isLinux() && !isNightly()) {
    todo(prefExists, "pref security.sandbox.content.level exists");
    if (!prefExists) {
      return;
    }
  }

  ok(prefExists, "pref security.sandbox.content.level exists");
  if (!prefExists) {
    return;
  }

  // Special case Linux on !isNightly
  if (isLinux() && !isNightly()) {
    todo(level > 0, "content sandbox enabled for !nightly.");
    return;
  }

  info(`security.sandbox.content.level=${level}`);
  ok(level > 0, "content sandbox is enabled.");
  if (level == 0) {
    info("content sandbox is not enabled, exiting");
    return;
  }

  let isFileIOSandboxed = isContentFileIOSandboxed(level);

  // Special case Linux on !isNightly
  if (isLinux() && !isNightly()) {
    todo(isFileIOSandboxed, "content file I/O sandbox enabled for !nightly.");
    return;
  }

  // Content sandbox enabled, but level doesn't include file I/O sandboxing.
  ok(isFileIOSandboxed, "content file I/O sandboxing is enabled.");
  if (!isFileIOSandboxed) {
    info("content sandbox level too low for file I/O tests, exiting\n");
    return;
  }

  let browser = gBrowser.selectedBrowser;

  {
    // test if the content process can create in $HOME, this should fail
    let homeFile = fileInHomeDir();
    let path = homeFile.path;
    let fileCreated = yield ContentTask.spawn(browser, path, createFile);
    ok(fileCreated == false, "creating a file in home dir is not permitted");
    if (fileCreated == true) {
      // content process successfully created the file, now remove it
      homeFile.remove(false);
    }
  }

  {
    // test if the content process can create a temp file, should pass
    let path = fileInTempDir().path;
    let fileCreated = yield ContentTask.spawn(browser, path, createFile);
    if (!fileCreated && isWin()) {
      // TODO: fix 1329294 and enable this test for Windows.
      // Not using todo() because this only fails on automation.
      info("ignoring failure to write to content temp due to 1329294\n");
      return;
    }
    ok(fileCreated == true, "creating a file in content temp is permitted");
    // now delete the file
    let fileDeleted = yield ContentTask.spawn(browser, path, deleteFile);
    ok(fileDeleted == true, "deleting a file in content temp is permitted");
  }
});
