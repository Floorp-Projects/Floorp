/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/utils.js");

// A wise line of Greek verse, and the utf-8 byte encoding.
// N.b., Greek begins at utf-8 ce 91
const TEST_STR = "πόλλ' οἶδ' ἀλώπηξ, ἀλλ' ἐχῖνος ἓν μέγα";
const TEST_HEX = h("cf 80 cf 8c ce bb ce bb   27 20 ce bf e1 bc b6 ce"+
                   "b4 27 20 e1 bc 80 ce bb   cf 8e cf 80 ce b7 ce be"+
                   "2c 20 e1 bc 80 ce bb ce   bb 27 20 e1 bc 90 cf 87"+
                   "e1 bf 96 ce bd ce bf cf   82 20 e1 bc 93 ce bd 20"+
                   "ce bc ce ad ce b3 ce b1");
// Integer byte values for the above
const TEST_BYTES = [207,128,207,140,206,187,206,187,
                     39, 32,206,191,225,188,182,206,
                    180, 39, 32,225,188,128,206,187,
                    207,142,207,128,206,183,206,190,
                     44, 32,225,188,128,206,187,206,
                    187, 39, 32,225,188,144,207,135,
                    225,191,150,206,189,206,191,207,
                    130, 32,225,188,147,206,189, 32,
                    206,188,206,173,206,179,206,177];

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

add_task(function test_stringAsHex() {
  do_check_eq(TEST_HEX, CommonUtils.stringAsHex(TEST_STR));
  run_next_test();
});

add_task(function test_hexAsString() {
  do_check_eq(TEST_STR, CommonUtils.hexAsString(TEST_HEX));
  run_next_test();
});

add_task(function test_hexToBytes() {
  let bytes = CommonUtils.hexToBytes(TEST_HEX);
  do_check_eq(TEST_BYTES.length, bytes.length);
  // Ensure that the decimal values of each byte are correct
  do_check_true(arraysEqual(TEST_BYTES,
      CommonUtils.stringToByteArray(bytes)));
  run_next_test();
});

add_task(function test_bytesToHex() {
  // Create a list of our character bytes from the reference int values
  let bytes = CommonUtils.byteArrayToString(TEST_BYTES);
  do_check_eq(TEST_HEX, CommonUtils.bytesAsHex(bytes));
  run_next_test();
});

add_task(function test_stringToBytes() {
  do_check_true(arraysEqual(TEST_BYTES,
      CommonUtils.stringToByteArray(CommonUtils.stringToBytes(TEST_STR))));
  run_next_test();
});

add_task(function test_stringRoundTrip() {
  do_check_eq(TEST_STR,
    CommonUtils.hexAsString(CommonUtils.stringAsHex(TEST_STR)));
  run_next_test();
});

add_task(function test_hexRoundTrip() {
  do_check_eq(TEST_HEX,
    CommonUtils.stringAsHex(CommonUtils.hexAsString(TEST_HEX)));
  run_next_test();
});

add_task(function test_byteArrayRoundTrip() {
  do_check_true(arraysEqual(TEST_BYTES,
    CommonUtils.stringToByteArray(CommonUtils.byteArrayToString(TEST_BYTES))));
  run_next_test();
});

// turn formatted test vectors into normal hex strings
function h(hexStr) {
  return hexStr.replace(/\s+/g, "");
}

function arraysEqual(a1, a2) {
  if (a1.length !== a2.length) {
    return false;
  }
  for (let i = 0; i < a1.length; i++) {
    if (a1[i] !== a2[i]) {
      return false;
    }
  }
  return true;
}
