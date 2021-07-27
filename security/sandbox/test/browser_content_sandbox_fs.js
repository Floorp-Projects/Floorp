/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from browser_content_sandbox_utils.js */
"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/" +
    "security/sandbox/test/browser_content_sandbox_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/" +
    "security/sandbox/test/browser_content_sandbox_fs_tests.js",
  this
);

/*
 * This test exercises file I/O from web and file content processes using
 * OS.File methods to validate that calls that are meant to be blocked by
 * content sandboxing are blocked.
 */

//
// Checks that sandboxing is enabled and at the appropriate level
// setting before triggering tests that do the file I/O.
//
// Tests attempting to write to a file in the home directory from the
// content process--expected to fail.
//
// Tests attempting to write to a file in the content temp directory
// from the content process--expected to succeed. Uses "ContentTmpD".
//
// Tests reading various files and directories from file and web
// content processes.
//
add_task(async function() {
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
    return;
  }

  // Test creating a file in the home directory from a web content process
  add_task(createFileInHome); // eslint-disable-line no-undef

  // Test creating a file content temp from a web content process
  add_task(createTempFile); // eslint-disable-line no-undef

  // Test reading files/dirs from web and file content processes
  add_task(testFileAccessAllPlatforms); // eslint-disable-line no-undef

  add_task(testFileAccessMacOnly); // eslint-disable-line no-undef

  add_task(testFileAccessLinuxOnly); // eslint-disable-line no-undef

  add_task(testFileAccessWindowsOnly); // eslint-disable-line no-undef

  add_task(cleanupBrowserTabs); // eslint-disable-line no-undef
});
