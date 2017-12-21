/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

do_register_cleanup(function() {
  Services.prefs.setBoolPref("toolkit.osfile.log", false);
});

function run_test() {
  Services.prefs.setBoolPref("toolkit.osfile.log", true);

  run_next_test();
}

add_task(async function() {
  // Set up profile. We create the directory in the profile, because the profile
  // is removed after every test run.
  do_get_profile();

  let file = OS.Path.join(OS.Constants.Path.profileDir, "file");
  let dir = OS.Path.join(OS.Constants.Path.profileDir, "directory");
  let file1 = OS.Path.join(dir, "file1");
  let file2 = OS.Path.join(dir, "file2");
  let subDir = OS.Path.join(dir, "subdir");
  let fileInSubDir = OS.Path.join(subDir, "file");

  // Sanity checking for the test
  Assert.equal(false, (await OS.File.exists(dir)));

  // Remove non-existent directory
  let exception = null;
  try {
    await OS.File.removeDir(dir, {ignoreAbsent: false});
  } catch (ex) {
    exception = ex;
  }

  Assert.ok(!!exception);
  Assert.ok(exception instanceof OS.File.Error);

  // Remove non-existent directory with ignoreAbsent
  await OS.File.removeDir(dir, {ignoreAbsent: true});
  await OS.File.removeDir(dir);

  // Remove file with ignoreAbsent: false
  await OS.File.writeAtomic(file, "content", { tmpPath: file + ".tmp" });
  exception = null;
  try {
    await OS.File.removeDir(file, {ignoreAbsent: false});
  } catch (ex) {
    exception = ex;
  }

  Assert.ok(!!exception);
  Assert.ok(exception instanceof OS.File.Error);

  // Remove empty directory
  await OS.File.makeDir(dir);
  await OS.File.removeDir(dir);
  Assert.equal(false, (await OS.File.exists(dir)));

  // Remove directory that contains one file
  await OS.File.makeDir(dir);
  await OS.File.writeAtomic(file1, "content", { tmpPath: file1 + ".tmp" });
  await OS.File.removeDir(dir);
  Assert.equal(false, (await OS.File.exists(dir)));

  // Remove directory that contains multiple files
  await OS.File.makeDir(dir);
  await OS.File.writeAtomic(file1, "content", { tmpPath: file1 + ".tmp" });
  await OS.File.writeAtomic(file2, "content", { tmpPath: file2 + ".tmp" });
  await OS.File.removeDir(dir);
  Assert.equal(false, (await OS.File.exists(dir)));

  // Remove directory that contains a file and a directory
  await OS.File.makeDir(dir);
  await OS.File.writeAtomic(file1, "content", { tmpPath: file1 + ".tmp" });
  await OS.File.makeDir(subDir);
  await OS.File.writeAtomic(fileInSubDir, "content", { tmpPath: fileInSubDir + ".tmp" });
  await OS.File.removeDir(dir);
  Assert.equal(false, (await OS.File.exists(dir)));
});

add_task(async function test_unix_symlink() {
  // Windows does not implement OS.File.unixSymLink()
  if (OS.Constants.Win) {
    return;
  }

  // Android / B2G file systems typically don't support symlinks.
  if (OS.Constants.Sys.Name == "Android") {
    return;
  }

  let file = OS.Path.join(OS.Constants.Path.profileDir, "file");
  let dir = OS.Path.join(OS.Constants.Path.profileDir, "directory");
  let file1 = OS.Path.join(dir, "file1");

  // This test will create the following directory structure:
  // <profileDir>/file                             (regular file)
  // <profileDir>/file.link => file                (symlink)
  // <profileDir>/directory                        (directory)
  // <profileDir>/linkdir => directory             (directory)
  // <profileDir>/directory/file1                  (regular file)
  // <profileDir>/directory3                       (directory)
  // <profileDir>/directory3/file3                 (directory)
  // <profileDir>/directory/link2 => ../directory3 (regular file)

  // Sanity checking for the test
  Assert.equal(false, (await OS.File.exists(dir)));

  await OS.File.writeAtomic(file, "content", { tmpPath: file + ".tmp" });
  Assert.ok((await OS.File.exists(file)));
  let info = await OS.File.stat(file, {unixNoFollowingLinks: true});
  Assert.ok(!info.isDir);
  Assert.ok(!info.isSymLink);

  await OS.File.unixSymLink(file, file + ".link");
  Assert.ok((await OS.File.exists(file + ".link")));
  info = await OS.File.stat(file + ".link", {unixNoFollowingLinks: true});
  Assert.ok(!info.isDir);
  Assert.ok(info.isSymLink);
  info = await OS.File.stat(file + ".link");
  Assert.ok(!info.isDir);
  Assert.ok(!info.isSymLink);
  await OS.File.remove(file + ".link");
  Assert.equal(false, (await OS.File.exists(file + ".link")));

  await OS.File.makeDir(dir);
  Assert.ok((await OS.File.exists(dir)));
  info = await OS.File.stat(dir, {unixNoFollowingLinks: true});
  Assert.ok(info.isDir);
  Assert.ok(!info.isSymLink);

  let link = OS.Path.join(OS.Constants.Path.profileDir, "linkdir");

  await OS.File.unixSymLink(dir, link);
  Assert.ok((await OS.File.exists(link)));
  info = await OS.File.stat(link, {unixNoFollowingLinks: true});
  Assert.ok(!info.isDir);
  Assert.ok(info.isSymLink);
  info = await OS.File.stat(link);
  Assert.ok(info.isDir);
  Assert.ok(!info.isSymLink);

  let dir3 = OS.Path.join(OS.Constants.Path.profileDir, "directory3");
  let file3 = OS.Path.join(dir3, "file3");
  let link2 = OS.Path.join(dir, "link2");

  await OS.File.writeAtomic(file1, "content", { tmpPath: file1 + ".tmp" });
  Assert.ok((await OS.File.exists(file1)));
  await OS.File.makeDir(dir3);
  Assert.ok((await OS.File.exists(dir3)));
  await OS.File.writeAtomic(file3, "content", { tmpPath: file3 + ".tmp" });
  Assert.ok((await OS.File.exists(file3)));
  await OS.File.unixSymLink("../directory3", link2);
  Assert.ok((await OS.File.exists(link2)));

  await OS.File.removeDir(link);
  Assert.equal(false, (await OS.File.exists(link)));
  Assert.ok((await OS.File.exists(file1)));
  await OS.File.removeDir(dir);
  Assert.equal(false, (await OS.File.exists(dir)));
  Assert.ok((await OS.File.exists(file3)));
  await OS.File.removeDir(dir3);
  Assert.equal(false, (await OS.File.exists(dir3)));

  // This task will be executed only on Unix-like systems.
  // Please do not add tests independent to operating systems here
  // or implement symlink() on Windows.
});
