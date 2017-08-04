/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that functions throw the appropriate exceptions.
 */

"use strict";

var EXISTING_FILE = do_get_file("xpcshell.ini").path;


// Tests on |open|

add_test_pair(async function test_typeerror() {
  let exn;
  try {
    let fd = await OS.File.open("/tmp", {no_such_key: 1});
    do_print("Fd: " + fd);
  } catch (ex) {
    exn = ex;
  }
  do_print("Exception: " + exn);
  do_check_true(exn.constructor.name == "TypeError");
});

// Tests on |read|

add_test_pair(async function test_bad_encoding() {
  do_print("Testing with a wrong encoding");
  try {
    await OS.File.read(EXISTING_FILE, { encoding: "baby-speak-encoded" });
    do_throw("Should have thrown with an ex.becauseInvalidArgument");
  } catch (ex) {
    if (ex.becauseInvalidArgument) {
      do_print("Wrong encoding caused the correct exception");
    } else {
      throw ex;
    }
  }

  try {
    await OS.File.read(EXISTING_FILE, { encoding: 4 });
    do_throw("Should have thrown a TypeError");
  } catch (ex) {
    if (ex.constructor.name == "TypeError") {
      // Note that TypeError doesn't carry across compartments
      do_print("Non-string encoding caused the correct exception");
    } else {
      throw ex;
    }
  }
});

add_test_pair(async function test_bad_compression() {
  do_print("Testing with a non-existing compression");
  try {
    await OS.File.read(EXISTING_FILE, { compression: "mmmh-crunchy" });
    do_throw("Should have thrown with an ex.becauseInvalidArgument");
  } catch (ex) {
    if (ex.becauseInvalidArgument) {
      do_print("Wrong encoding caused the correct exception");
    } else {
      throw ex;
    }
  }

  do_print("Testing with a bad type for option compression");
  try {
    await OS.File.read(EXISTING_FILE, { compression: 5 });
    do_throw("Should have thrown a TypeError");
  } catch (ex) {
    if (ex.constructor.name == "TypeError") {
      // Note that TypeError doesn't carry across compartments
      do_print("Non-string encoding caused the correct exception");
    } else {
      throw ex;
    }
  }
});

add_test_pair(async function test_bad_bytes() {
  do_print("Testing with a bad type for option bytes");
  try {
    await OS.File.read(EXISTING_FILE, { bytes: "five" });
    do_throw("Should have thrown a TypeError");
  } catch (ex) {
    if (ex.constructor.name == "TypeError") {
      // Note that TypeError doesn't carry across compartments
      do_print("Non-number bytes caused the correct exception");
    } else {
      throw ex;
    }
  }
});

add_test_pair(async function read_non_existent() {
  do_print("Testing with a non-existent file");
  try {
    await OS.File.read("I/do/not/exist");
    do_throw("Should have thrown with an ex.becauseNoSuchFile");
  } catch (ex) {
    if (ex.becauseNoSuchFile) {
      do_print("Correct exceptions");
    } else {
      throw ex;
    }
  }
});

function run_test() {
  run_next_test();
}
