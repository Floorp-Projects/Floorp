/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
var SHARED_PATH;

add_task(async function init() {
  do_get_profile();
  SHARED_PATH = OS.Path.join(
    OS.Constants.Path.profileDir,
    "test_osfile_write_zerobytes.tmp"
  );
});

add_test_pair(async function test_osfile_writeAtomic_zerobytes() {
  let encoder = new TextEncoder();
  let string1 = "";
  let outbin = encoder.encode(string1);
  await OS.File.writeAtomic(SHARED_PATH, outbin);

  let decoder = new TextDecoder();
  let bin = await OS.File.read(SHARED_PATH);
  let string2 = decoder.decode(bin);
  // Checking if writeAtomic supports writing encoded zero-byte strings
  Assert.equal(string2, string1, "Read the expected (empty) string.");
});
