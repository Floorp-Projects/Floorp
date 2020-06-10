/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for the "DownloadPaths.jsm" JavaScript module.
 */

function testSanitize(leafName, expectedLeafName, options = {}) {
  Assert.equal(DownloadPaths.sanitize(leafName, options), expectedLeafName);
}

function testSplitBaseNameAndExtension(aLeafName, [aBase, aExt]) {
  var [base, ext] = DownloadPaths.splitBaseNameAndExtension(aLeafName);
  Assert.equal(base, aBase);
  Assert.equal(ext, aExt);

  // If we modify the base name and concatenate it with the extension again,
  // another roundtrip through the function should give a consistent result.
  // The only exception is when we introduce an extension in a file name that
  // didn't have one or that ended with one of the special cases like ".gz". If
  // we avoid using a dot and we introduce at least another special character,
  // the results are always consistent.
  [base, ext] = DownloadPaths.splitBaseNameAndExtension("(" + base + ")" + ext);
  Assert.equal(base, "(" + aBase + ")");
  Assert.equal(ext, aExt);
}

function testCreateNiceUniqueFile(aTempFile, aExpectedLeafName) {
  var createdFile = DownloadPaths.createNiceUniqueFile(aTempFile);
  Assert.equal(createdFile.leafName, aExpectedLeafName);
}

add_task(async function test_sanitize() {
  // Platform-dependent conversion of special characters to spaces.
  const kSpecialChars = 'A:*?|""<<>>;,+=[]B][=+,;>><<""|?*:C';
  if (AppConstants.platform == "android") {
    testSanitize(kSpecialChars, "A B C");
    testSanitize(" :: Website :: ", "Website");
    testSanitize("* Website!", "Website!");
    testSanitize("Website | Page!", "Website Page!");
    testSanitize("Directory Listing: /a/b/", "Directory Listing _a_b_");
  } else if (AppConstants.platform == "win") {
    testSanitize(kSpecialChars, "A ''(());,+=[]B][=+,;))(('' C");
    testSanitize(" :: Website :: ", "Website");
    testSanitize("* Website!", "Website!");
    testSanitize("Website | Page!", "Website Page!");
    testSanitize("Directory Listing: /a/b/", "Directory Listing _a_b_");
  } else if (AppConstants.platform == "macosx") {
    testSanitize(kSpecialChars, 'A *?|""<<>>;,+=[]B][=+,;>><<""|?* C');
    testSanitize(" :: Website :: ", "Website");
    testSanitize("* Website!", "* Website!");
    testSanitize("Website | Page!", "Website | Page!");
    testSanitize("Directory Listing: /a/b/", "Directory Listing _a_b_");
  } else {
    testSanitize(kSpecialChars, kSpecialChars.replace(/[:]/g, " "));
    testSanitize(" :: Website :: ", "Website");
    testSanitize("* Website!", "* Website!");
    testSanitize("Website | Page!", "Website | Page!");
    testSanitize("Directory Listing: /a/b/", "Directory Listing _a_b_");
  }

  // Conversion of consecutive runs of slashes and backslashes to underscores.
  testSanitize("\\ \\\\Website\\/Page// /", "_ _Website_Page_ _");

  // Removal of leading and trailing whitespace and dots after conversion.
  testSanitize("  Website  ", "Website");
  testSanitize(". . Website . Page . .", "Website . Page");
  testSanitize(" File . txt ", "File . txt");
  testSanitize("\f\n\r\t\v\x00\x1f\x7f\x80\x9f\xa0 . txt", "txt");
  testSanitize("\u1680\u180e\u2000\u2008\u200a . txt", "txt");
  testSanitize("\u2028\u2029\u202f\u205f\u3000\ufeff . txt", "txt");

  // Strings with whitespace and dots only.
  testSanitize(".", "");
  testSanitize("..", "");
  testSanitize(" ", "");
  testSanitize(" . ", "");

  // Stripping of BIDI formatting characters.
  testSanitize("\u200e \u202b\u202c\u202d\u202etest\x7f\u200f", "test");
  testSanitize("AB\x7f\u202a\x7f\u202a\x7fCD", "AB CD");

  // Stripping of colons:
  testSanitize("foo:bar", "foo bar");

  // not compressing whitespaces.
  testSanitize("foo : bar", "foo   bar", { compressWhitespaces: false });
});

add_task(async function test_splitBaseNameAndExtension() {
  // Usual file names.
  testSplitBaseNameAndExtension("base", ["base", ""]);
  testSplitBaseNameAndExtension("base.ext", ["base", ".ext"]);
  testSplitBaseNameAndExtension("base.application", ["base", ".application"]);
  testSplitBaseNameAndExtension("base.x.Z", ["base", ".x.Z"]);
  testSplitBaseNameAndExtension("base.ext.Z", ["base", ".ext.Z"]);
  testSplitBaseNameAndExtension("base.ext.gz", ["base", ".ext.gz"]);
  testSplitBaseNameAndExtension("base.ext.Bz2", ["base", ".ext.Bz2"]);
  testSplitBaseNameAndExtension("base..ext", ["base.", ".ext"]);
  testSplitBaseNameAndExtension("base..Z", ["base.", ".Z"]);
  testSplitBaseNameAndExtension("base. .Z", ["base. ", ".Z"]);
  testSplitBaseNameAndExtension("base.base.Bz2", ["base.base", ".Bz2"]);
  testSplitBaseNameAndExtension("base  .ext", ["base  ", ".ext"]);

  // Corner cases. A name ending with a dot technically has no extension, but
  // we consider the ending dot separately from the base name so that modifying
  // the latter never results in an extension being introduced accidentally.
  // Names beginning with a dot are hidden files on Unix-like platforms and if
  // their name doesn't contain another dot they should have no extension, but
  // on Windows the whole name is considered as an extension.
  testSplitBaseNameAndExtension("base.", ["base", "."]);
  testSplitBaseNameAndExtension(".ext", ["", ".ext"]);

  // Unusual file names (not recommended as input to the function).
  testSplitBaseNameAndExtension("base. ", ["base", ". "]);
  testSplitBaseNameAndExtension("base ", ["base ", ""]);
  testSplitBaseNameAndExtension("", ["", ""]);
  testSplitBaseNameAndExtension(" ", [" ", ""]);
  testSplitBaseNameAndExtension(" . ", [" ", ". "]);
  testSplitBaseNameAndExtension(" .. ", [" .", ". "]);
  testSplitBaseNameAndExtension(" .ext", [" ", ".ext"]);
  testSplitBaseNameAndExtension(" .ext. ", [" .ext", ". "]);
  testSplitBaseNameAndExtension(" .ext.gz ", [" .ext", ".gz "]);
});

add_task(async function test_createNiceUniqueFile() {
  var destDir = FileTestUtils.getTempFile("destdir");
  destDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

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
  Assert.ok(newFile.leafName.length < tempFile.leafName.length);
  Assert.equal(newFile.leafName.slice(-4), ".txt");

  // Creating a valid file name from an invalid one is not always possible.
  tempFile.append("file-under-long-directory.txt");
  try {
    DownloadPaths.createNiceUniqueFile(tempFile);
    do_throw("Exception expected with a long parent directory name.");
  } catch (e) {
    // An exception is expected, but we don't know which one exactly.
  }
});
