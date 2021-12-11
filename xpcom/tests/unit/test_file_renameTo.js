/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function run_test() {
  // Create the base directory.
  let base = Services.dirsvc.get("TmpD", Ci.nsIFile);
  base.append("renameTesting");
  if (base.exists()) {
    base.remove(true);
  }
  base.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0777", 8));

  // Create a sub directory under the base.
  let subdir = base.clone();
  subdir.append("subdir");
  subdir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0777", 8));

  // Create a file under the sub directory.
  let tempFile = subdir.clone();
  tempFile.append("file0.txt");
  tempFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0777", 8));

  // Test renameTo in the base directory
  tempFile.renameTo(null, "file1.txt");
  Assert.ok(exists(subdir, "file1.txt"));

  // Test moving across directories
  tempFile = subdir.clone();
  tempFile.append("file1.txt");
  tempFile.renameTo(base, "");
  Assert.ok(exists(base, "file1.txt"));

  // Test moving across directories and renaming at the same time
  tempFile = base.clone();
  tempFile.append("file1.txt");
  tempFile.renameTo(subdir, "file2.txt");
  Assert.ok(exists(subdir, "file2.txt"));

  // Test moving a directory
  subdir.renameTo(base, "renamed");
  Assert.ok(exists(base, "renamed"));
  let renamed = base.clone();
  renamed.append("renamed");
  Assert.ok(exists(renamed, "file2.txt"));

  base.remove(true);
}

function exists(parent, filename) {
  let file = parent.clone();
  file.append(filename);
  return file.exists();
}
