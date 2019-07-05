/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/ctypes.jsm", this);
ChromeUtils.import("resource://testing-common/AppData.jsm", this);

function compare_paths(ospath, key) {
  let file;
  try {
    file = Services.dirsvc.get(key, Ci.nsIFile);
  } catch (ex) {}

  if (file) {
    Assert.ok(!!ospath);
    Assert.equal(ospath, file.path);
  } else {
    info(
      "WARNING: " + key + " is not defined. Test may not be testing anything!"
    );
    Assert.ok(!ospath);
  }
}

// Test simple paths
add_task(async function test_simple_paths() {
  Assert.ok(!!OS.Constants.Path.tmpDir);
  compare_paths(OS.Constants.Path.tmpDir, "TmpD");
});

// Some path constants aren't set up until the profile is available. This
// test verifies that behavior.
add_task(async function test_before_after_profile() {
  Assert.equal(null, OS.Constants.Path.profileDir);
  Assert.equal(null, OS.Constants.Path.localProfileDir);
  Assert.equal(null, OS.Constants.Path.userApplicationDataDir);

  do_get_profile();
  Assert.ok(!!OS.Constants.Path.profileDir);
  Assert.ok(!!OS.Constants.Path.localProfileDir);

  // UAppData is still null because the xpcshell profile doesn't set it up.
  // This test is mostly here to fail in case behavior of do_get_profile() ever
  // changes. We want to know if our assumptions no longer hold!
  Assert.equal(null, OS.Constants.Path.userApplicationDataDir);

  await makeFakeAppDir();
  Assert.ok(!!OS.Constants.Path.userApplicationDataDir);

  // FUTURE: verify AppData too (bug 964291).
});

// Test presence of paths that only exist on Desktop platforms
add_task(async function test_desktop_paths() {
  if (OS.Constants.Sys.Name == "Android") {
    return;
  }
  Assert.ok(!!OS.Constants.Path.homeDir);

  compare_paths(OS.Constants.Path.homeDir, "Home");
  compare_paths(OS.Constants.Path.userApplicationDataDir, "UAppData");

  compare_paths(OS.Constants.Path.macUserLibDir, "ULibDir");
});

// Open libxul
add_task(async function test_libxul() {
  ctypes.open(OS.Constants.Path.libxul);
  info("Linked to libxul");
});
