/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/utils.js");


function run_test() {
  run_next_test();
}

add_test(function test_compress_string() {
  const INPUT = "hello";

  let result = CommonUtils.convertString(INPUT, "uncompressed", "deflate");
  do_check_eq(result.length, 13);

  let result2 = CommonUtils.convertString(INPUT, "uncompressed", "deflate");
  do_check_eq(result, result2);

  let result3 = CommonUtils.convertString(result, "deflate", "uncompressed");
  do_check_eq(result3, INPUT);

  run_next_test();
});

add_test(function test_compress_utf8() {
  const INPUT = "Árvíztűrő tükörfúrógép いろはにほへとちりぬるを Pijamalı hasta, yağız şoföre çabucak güvendi.";
  let inputUTF8 = CommonUtils.encodeUTF8(INPUT);

  let compressed = CommonUtils.convertString(inputUTF8, "uncompressed", "deflate");
  let uncompressed = CommonUtils.convertString(compressed, "deflate", "uncompressed");

  do_check_eq(uncompressed, inputUTF8);

  let outputUTF8 = CommonUtils.decodeUTF8(uncompressed);
  do_check_eq(outputUTF8, INPUT);

  run_next_test();
});

add_test(function test_bad_argument() {
  let failed = false;
  try {
    CommonUtils.convertString(null, "uncompressed", "deflate");
  } catch (ex) {
    failed = true;
    do_check_true(ex.message.startsWith("Input string must be defined"));
  } finally {
    do_check_true(failed);
  }

  run_next_test();
});
