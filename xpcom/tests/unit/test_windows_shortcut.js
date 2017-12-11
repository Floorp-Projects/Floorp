/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const LocalFile = CC("@mozilla.org/file/local;1", "nsIFile", "initWithPath");

Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  // This test makes sense only on Windows, so skip it on other platforms
  if ("nsILocalFileWin" in Ci
   && do_get_cwd() instanceof Ci.nsILocalFileWin) {

    let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
    tempDir.append("shortcutTesting");
    tempDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o666);

    test_create_noargs(tempDir);
    test_create_notarget(tempDir);
    test_create_targetonly(tempDir);
    test_create_normal(tempDir);
    test_create_unicode(tempDir);

    test_update_noargs(tempDir);
    test_update_notarget(tempDir);
    test_update_targetonly(tempDir);
    test_update_normal(tempDir);
    test_update_unicode(tempDir);
  }
}

function test_create_noargs(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("shouldNeverExist.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  try {
    win.setShortcut();
    do_throw("Creating a shortcut with no args (no target) should throw");
  } catch (e) {
    if (!(e instanceof Ci.nsIException
          && e.result == Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)) {
      throw e;
    }
  }
}

function test_create_notarget(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("shouldNeverExist2.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  try {
    win.setShortcut(null,
                    do_get_cwd(),
                    "arg1 arg2",
                    "Shortcut with no target");
    do_throw("Creating a shortcut with no target should throw");
  } catch (e) {
    if (!(e instanceof Ci.nsIException
          && e.result == Cr.NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)) {
      throw e;
    }
  }
}

function test_create_targetonly(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("createdShortcut.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let targetFile = tempDir.clone();
  targetFile.append("shortcutTarget.exe");
  targetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  win.setShortcut(targetFile);

  let shortcutTarget = LocalFile(shortcutFile.target);
  do_check_true(shortcutTarget.equals(targetFile));
}

function test_create_normal(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("createdShortcut.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let targetFile = tempDir.clone();
  targetFile.append("shortcutTarget.exe");
  targetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  win.setShortcut(targetFile,
                  do_get_cwd(),
                  "arg1 arg2",
                  "Ordinary shortcut");

  let shortcutTarget = LocalFile(shortcutFile.target);
  do_check_true(shortcutTarget.equals(targetFile));
}

function test_create_unicode(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("createdShortcut.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let targetFile = tempDir.clone();
  targetFile.append("ṩhогТϾừ†Target.exe");
  targetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  win.setShortcut(targetFile,
                  do_get_cwd(), // XXX: This should probably be a unicode dir
                  "ᾶṟǵ1 ᾶṟǵ2",
                  "ῧṋіḉѻₑ");

  let shortcutTarget = LocalFile(shortcutFile.target);
  do_check_true(shortcutTarget.equals(targetFile));
}

function test_update_noargs(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("createdShortcut.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let targetFile = tempDir.clone();
  targetFile.append("shortcutTarget.exe");
  targetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  win.setShortcut(targetFile,
                  do_get_cwd(),
                  "arg1 arg2",
                  "A sample shortcut");

  win.setShortcut();

  let shortcutTarget = LocalFile(shortcutFile.target);
  do_check_true(shortcutTarget.equals(targetFile));
}

function test_update_notarget(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("createdShortcut.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let targetFile = tempDir.clone();
  targetFile.append("shortcutTarget.exe");
  targetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  win.setShortcut(targetFile,
                  do_get_cwd(),
                  "arg1 arg2",
                  "A sample shortcut");

  win.setShortcut(null,
                  do_get_profile(),
                  "arg3 arg4",
                  "An UPDATED shortcut");

  let shortcutTarget = LocalFile(shortcutFile.target);
  do_check_true(shortcutTarget.equals(targetFile));
}

function test_update_targetonly(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("createdShortcut.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let targetFile = tempDir.clone();
  targetFile.append("shortcutTarget.exe");
  targetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  win.setShortcut(targetFile,
                  do_get_cwd(),
                  "arg1 arg2",
                  "A sample shortcut");

  let newTargetFile = tempDir.clone();
  newTargetFile.append("shortcutTarget.exe");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  win.setShortcut(newTargetFile);

  let shortcutTarget = LocalFile(shortcutFile.target);
  do_check_true(shortcutTarget.equals(newTargetFile));
}

function test_update_normal(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("createdShortcut.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let targetFile = tempDir.clone();
  targetFile.append("shortcutTarget.exe");
  targetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  win.setShortcut(targetFile,
                  do_get_cwd(),
                  "arg1 arg2",
                  "A sample shortcut");

  let newTargetFile = tempDir.clone();
  newTargetFile.append("shortcutTarget.exe");
  newTargetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  win.setShortcut(newTargetFile,
                  do_get_profile(),
                  "arg3 arg4",
                  "An UPDATED shortcut");

  let shortcutTarget = LocalFile(shortcutFile.target);
  do_check_true(shortcutTarget.equals(newTargetFile));
}

function test_update_unicode(tempDir) {
  let shortcutFile = tempDir.clone();
  shortcutFile.append("createdShortcut.lnk");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let targetFile = tempDir.clone();
  targetFile.append("shortcutTarget.exe");
  targetFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let win = shortcutFile.QueryInterface(Ci.nsILocalFileWin);

  win.setShortcut(targetFile,
                  do_get_cwd(),
                  "arg1 arg2",
                  "A sample shortcut");

  let newTargetFile = tempDir.clone();
  newTargetFile.append("ṩhогТϾừ†Target.exe");
  shortcutFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  win.setShortcut(newTargetFile,
                  do_get_profile(), // XXX: This should probably be unicode
                  "ᾶṟǵ3 ᾶṟǵ4",
                  "A ῧṋіḉѻₑ shortcut");

  let shortcutTarget = LocalFile(shortcutFile.target);
  do_check_true(shortcutTarget.equals(newTargetFile));
}
