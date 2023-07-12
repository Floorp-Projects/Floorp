/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test checks against failures that may occur while creating and/or
 * renaming files with Unicode paths on Windows.
 * See bug 1063635#c89 for a failure due to a Unicode filename being renamed.
 */

"use strict";
var profileDir;

async function writeAndCheck(path, tmpPath) {
  const encoder = new TextEncoder();
  const content = "tmpContent";
  const outBin = encoder.encode(content);
  await OS.File.writeAtomic(path, outBin, { tmpPath });

  const decoder = new TextDecoder();
  const writtenBin = await OS.File.read(path);
  const written = decoder.decode(writtenBin);

  // Clean up
  await OS.File.remove(path);
  Assert.equal(
    written,
    content,
    `Expected correct write/read for ${path} with tmpPath ${tmpPath}`
  );
}

add_task(async function init() {
  do_get_profile();
  profileDir = OS.Constants.Path.profileDir;
});

add_test_pair(async function test_osfile_writeAtomic_unicode_filename() {
  await writeAndCheck(OS.Path.join(profileDir, "☕") + ".tmp", undefined);
  await writeAndCheck(OS.Path.join(profileDir, "☕"), undefined);
  await writeAndCheck(
    OS.Path.join(profileDir, "☕") + ".tmp",
    OS.Path.join(profileDir, "☕")
  );
  await writeAndCheck(
    OS.Path.join(profileDir, "☕"),
    OS.Path.join(profileDir, "☕") + ".tmp"
  );
});
