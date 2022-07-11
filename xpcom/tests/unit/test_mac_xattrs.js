/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

const ATTR = "bogus.attr";
const VALUE = new TextEncoder().encode("bogus");

async function run_test() {
  const path = PathUtils.join(
    Services.dirsvc.get("TmpD", Ci.nsIFile).path,
    "macos-xattrs.tmp.d"
  );
  await IOUtils.makeDirectory(path);

  try {
    await test_macos_xattr(path);
  } finally {
    await IOUtils.remove(path, { recursive: true });
  }
}

async function test_macos_xattr(tmpDir) {
  const path = PathUtils.join(tmpDir, "file.tmp");

  await IOUtils.writeUTF8(path, "");

  const file = new FileUtils.File(path);
  file.queryInterface(Cc.nsILocalFileMac);

  Assert.ok(!file.exists(), "File should not exist");

  info("Testing reading an attribute on a file that does not exist");
  Assert.throws(
    () => file.hasXAttr(ATTR),
    /NS_ERROR_FILE_NOT_FOUND/,
    "Non-existant files can't have attributes checked"
  );
  Assert.throws(
    () => file.getXAttr(ATTR),
    /NS_ERROR_FILE_NOT_FOUND/,
    "Non-existant files can't have attributes read"
  );

  file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
  Assert.ok(file.exists(), "File exists after creation");

  info("Testing reading an attribute that does not exist");
  Assert.ok(
    !file.hasXAttr(ATTR),
    "File should not have attribute before being set."
  );
  Assert.throws(
    () => file.getXAttr(ATTR),
    /NS_ERROR_NOT_AVAILABLE/,
    "Attempting to get an attribute that does not exist throws"
  );

  {
    info("Testing setting and reading an attribute");
    file.setXAttr(ATTR, VALUE);
    Assert.ok(
      file.hasXAttr(ATTR),
      "File should have attribute after being set"
    );
    const result = file.getXAttr(ATTR);

    Assert.deepEqual(
      Array.from(result),
      Array.from(VALUE),
      "File should have attribute value matching what was set"
    );
  }

  info("Testing removing an attribute");
  file.delXAttr(ATTR);
  Assert.ok(
    !file.hasXAttr(ATTR),
    "File should no longer have the attribute after removal"
  );
  Assert.throws(
    () => file.getXAttr(ATTR),
    /NS_ERROR_NOT_AVAILABLE/,
    "Attempting to get an attribute after removal results in an error"
  );

  info("Testing removing an attribute that does not exist");
  Assert.throws(
    () => file.delXAttr(ATTR),
    /NS_ERROR_NOT_AVAILABLE/,
    "Attempting to remove an attribute that does not exist throws"
  );
}
