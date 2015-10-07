/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* ***** BEGIN LICENSE BLOCK *****
 *
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Tests for the "DownloadPaths.jsm" JavaScript module.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/DownloadPaths.jsm");

/**
 * Provides a temporary save directory.
 *
 * @returns nsIFile pointing to the new or existing directory.
 */
function createTemporarySaveDirectory()
{
  var saveDir = Cc["@mozilla.org/file/directory_service;1"].
                getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  }
  return saveDir;
}

function testSplitBaseNameAndExtension(aLeafName, [aBase, aExt])
{
  var [base, ext] = DownloadPaths.splitBaseNameAndExtension(aLeafName);
  do_check_eq(base, aBase);
  do_check_eq(ext, aExt);

  // If we modify the base name and concatenate it with the extension again,
  // another roundtrip through the function should give a consistent result.
  // The only exception is when we introduce an extension in a file name that
  // didn't have one or that ended with one of the special cases like ".gz". If
  // we avoid using a dot and we introduce at least another special character,
  // the results are always consistent.
  [base, ext] = DownloadPaths.splitBaseNameAndExtension("(" + base + ")" + ext);
  do_check_eq(base, "(" + aBase + ")");
  do_check_eq(ext, aExt);
}

function testCreateNiceUniqueFile(aTempFile, aExpectedLeafName)
{
  var createdFile = DownloadPaths.createNiceUniqueFile(aTempFile);
  do_check_eq(createdFile.leafName, aExpectedLeafName);
}

function run_test()
{
  // Usual file names.
  testSplitBaseNameAndExtension("base",             ["base", ""]);
  testSplitBaseNameAndExtension("base.ext",         ["base", ".ext"]);
  testSplitBaseNameAndExtension("base.application", ["base", ".application"]);
  testSplitBaseNameAndExtension("base.x.Z",         ["base", ".x.Z"]);
  testSplitBaseNameAndExtension("base.ext.Z",       ["base", ".ext.Z"]);
  testSplitBaseNameAndExtension("base.ext.gz",      ["base", ".ext.gz"]);
  testSplitBaseNameAndExtension("base.ext.Bz2",     ["base", ".ext.Bz2"]);
  testSplitBaseNameAndExtension("base..ext",        ["base.", ".ext"]);
  testSplitBaseNameAndExtension("base..Z",          ["base.", ".Z"]);
  testSplitBaseNameAndExtension("base. .Z",         ["base. ", ".Z"]);
  testSplitBaseNameAndExtension("base.base.Bz2",    ["base.base", ".Bz2"]);
  testSplitBaseNameAndExtension("base  .ext",       ["base  ", ".ext"]);

  // Corner cases. A name ending with a dot technically has no extension, but
  // we consider the ending dot separately from the base name so that modifying
  // the latter never results in an extension being introduced accidentally.
  // Names beginning with a dot are hidden files on Unix-like platforms and if
  // their name doesn't contain another dot they should have no extension, but
  // on Windows the whole name is considered as an extension.
  testSplitBaseNameAndExtension("base.",            ["base", "."]);
  testSplitBaseNameAndExtension(".ext",             ["", ".ext"]);

  // Unusual file names (not recommended as input to the function).
  testSplitBaseNameAndExtension("base. ",           ["base", ". "]);
  testSplitBaseNameAndExtension("base ",            ["base ", ""]);
  testSplitBaseNameAndExtension("",                 ["", ""]);
  testSplitBaseNameAndExtension(" ",                [" ", ""]);
  testSplitBaseNameAndExtension(" . ",              [" ", ". "]);
  testSplitBaseNameAndExtension(" .. ",             [" .", ". "]);
  testSplitBaseNameAndExtension(" .ext",            [" ", ".ext"]);
  testSplitBaseNameAndExtension(" .ext. ",          [" .ext", ". "]);
  testSplitBaseNameAndExtension(" .ext.gz ",        [" .ext", ".gz "]);

  var destDir = createTemporarySaveDirectory();
  try {
    // Single extension.
    var tempFile = destDir.clone();
    tempFile.append("test.txt");
    testCreateNiceUniqueFile(tempFile, "test.txt");
    testCreateNiceUniqueFile(tempFile, "test(1).txt");
    testCreateNiceUniqueFile(tempFile, "test(2).txt");

    // Double extension.
    tempFile.leafName = "test.tar.gz";
    testCreateNiceUniqueFile(tempFile, "test.tar.gz");
    testCreateNiceUniqueFile(tempFile, "test(1).tar.gz");
    testCreateNiceUniqueFile(tempFile, "test(2).tar.gz");

    // Test automatic shortening of long file names. We don't know exactly how
    // many characters are removed, because it depends on the name of the folder
    // where the file is located.
    tempFile.leafName = new Array(256).join("T") + ".txt";
    var newFile = DownloadPaths.createNiceUniqueFile(tempFile);
    do_check_true(newFile.leafName.length < tempFile.leafName.length);
    do_check_eq(newFile.leafName.slice(-4), ".txt");

    // Creating a valid file name from an invalid one is not always possible.
    tempFile.append("file-under-long-directory.txt");
    try {
      DownloadPaths.createNiceUniqueFile(tempFile);
      do_throw("Exception expected with a long parent directory name.")
    } catch (e) {
      // An exception is expected, but we don't know which one exactly.
    }
  } finally {
    // Clean up the temporary directory.
    destDir.remove(true);
  }
}
