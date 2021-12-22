/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Assert } = ChromeUtils.import("resource://testing-common/Assert.jsm");
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

async function run_test() {
  const path = PathUtils.join(await IOUtils.getTempDir(), "macos-xattrs.tmp.d");
  await IOUtils.makeDirectory(path);

  try {
    test_macos_xattr(path);
  } finally {
    await IOUtils.remove(path, { recursive: true });
  }
}

async function test_macos_xattr(tmpDir) {
  const encoder = new TextEncoder();
  const path = PathUtils.join(tmpDir, "file.tmp");

  await IOUtils.writeUTF8(path, "");

  const file = new FileUtils.File(path);
  file.queryInterface(Cc.nsILocalFileMac);

  info("Testing reading an attribute that does not exist");
  Assert.ok(
    !file.hasXAttr("bogus.attr"),
    "File should not have attribute before being set."
  );
  Assert.throws(
    () => file.getXAttr("bogus.attr"),
    /NS_ERROR_NOT_AVAILABLE/,
    "Attempting to get an attribute that does not exist throws"
  );

  {
    info("Testing setting and reading an attribute");
    file.setXAttr("bogus.attr", encoder.encode("bogus"));
    Assert.ok(
      file.hasXAttr("bogus.attr"),
      "File should have attribute after being set"
    );
    const result = file.getXAttr("bogus.attr");

    Assert.equal(
      result,
      encoder.encode("bogus"),
      "File should have attribute value matching what was set"
    );
  }

  info("Testing removing an attribute");
  file.delXAttr("bogus.attr");
  Assert.ok(
    !file.hasXAttr("bogus.attr"),
    "File should no longer have the attribute after removal"
  );
  Assert.throws(
    () => file.getXAttr("bogus.attr"),
    /NS_ERROR_NOT_AVAILABLE/,
    "Attempting to get an attribute after removal results in an error"
  );

  info("Testing removing an attribute that does not exist");
  Assert.throws(
    () => file.delXAttr("bogus.attr"),
    /NS_ERROR_NOT_AVAILABLE/,
    "Attempting to remove an attribute that does not exist throws"
  );
}
