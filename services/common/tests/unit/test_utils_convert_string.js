/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A wise line of Greek verse, and the utf-8 byte encoding.
// N.b., Greek begins at utf-8 ce 91
const TEST_STR = "πόλλ' οἶδ' ἀλώπηξ, ἀλλ' ἐχῖνος ἓν μέγα";
const TEST_HEX = h(
  "cf 80 cf 8c ce bb ce bb   27 20 ce bf e1 bc b6 ce" +
    "b4 27 20 e1 bc 80 ce bb   cf 8e cf 80 ce b7 ce be" +
    "2c 20 e1 bc 80 ce bb ce   bb 27 20 e1 bc 90 cf 87" +
    "e1 bf 96 ce bd ce bf cf   82 20 e1 bc 93 ce bd 20" +
    "ce bc ce ad ce b3 ce b1"
);
// Integer byte values for the above
const TEST_BYTES = [
  207,
  128,
  207,
  140,
  206,
  187,
  206,
  187,
  39,
  32,
  206,
  191,
  225,
  188,
  182,
  206,
  180,
  39,
  32,
  225,
  188,
  128,
  206,
  187,
  207,
  142,
  207,
  128,
  206,
  183,
  206,
  190,
  44,
  32,
  225,
  188,
  128,
  206,
  187,
  206,
  187,
  39,
  32,
  225,
  188,
  144,
  207,
  135,
  225,
  191,
  150,
  206,
  189,
  206,
  191,
  207,
  130,
  32,
  225,
  188,
  147,
  206,
  189,
  32,
  206,
  188,
  206,
  173,
  206,
  179,
  206,
  177,
];

add_test(function test_compress_string() {
  const INPUT = "hello";

  let result = CommonUtils.convertString(INPUT, "uncompressed", "deflate");
  Assert.equal(result.length, 13);

  let result2 = CommonUtils.convertString(INPUT, "uncompressed", "deflate");
  Assert.equal(result, result2);

  let result3 = CommonUtils.convertString(result, "deflate", "uncompressed");
  Assert.equal(result3, INPUT);

  run_next_test();
});

add_test(function test_compress_utf8() {
  const INPUT =
    "Árvíztűrő tükörfúrógép いろはにほへとちりぬるを Pijamalı hasta, yağız şoföre çabucak güvendi.";
  let inputUTF8 = CommonUtils.encodeUTF8(INPUT);

  let compressed = CommonUtils.convertString(
    inputUTF8,
    "uncompressed",
    "deflate"
  );
  let uncompressed = CommonUtils.convertString(
    compressed,
    "deflate",
    "uncompressed"
  );

  Assert.equal(uncompressed, inputUTF8);

  let outputUTF8 = CommonUtils.decodeUTF8(uncompressed);
  Assert.equal(outputUTF8, INPUT);

  run_next_test();
});

add_test(function test_bad_argument() {
  let failed = false;
  try {
    CommonUtils.convertString(null, "uncompressed", "deflate");
  } catch (ex) {
    failed = true;
    Assert.ok(ex.message.startsWith("Input string must be defined"));
  } finally {
    Assert.ok(failed);
  }

  run_next_test();
});

add_task(function test_stringAsHex() {
  Assert.equal(TEST_HEX, CommonUtils.stringAsHex(TEST_STR));
});

add_task(function test_hexAsString() {
  Assert.equal(TEST_STR, CommonUtils.hexAsString(TEST_HEX));
});

add_task(function test_hexToBytes() {
  let bytes = CommonUtils.hexToBytes(TEST_HEX);
  Assert.equal(TEST_BYTES.length, bytes.length);
  // Ensure that the decimal values of each byte are correct
  Assert.ok(arraysEqual(TEST_BYTES, CommonUtils.stringToByteArray(bytes)));
});

add_task(function test_bytesToHex() {
  // Create a list of our character bytes from the reference int values
  let bytes = CommonUtils.byteArrayToString(TEST_BYTES);
  Assert.equal(TEST_HEX, CommonUtils.bytesAsHex(bytes));
});

add_task(function test_stringToBytes() {
  Assert.ok(
    arraysEqual(
      TEST_BYTES,
      CommonUtils.stringToByteArray(CommonUtils.stringToBytes(TEST_STR))
    )
  );
});

add_task(function test_stringRoundTrip() {
  Assert.equal(
    TEST_STR,
    CommonUtils.hexAsString(CommonUtils.stringAsHex(TEST_STR))
  );
});

add_task(function test_hexRoundTrip() {
  Assert.equal(
    TEST_HEX,
    CommonUtils.stringAsHex(CommonUtils.hexAsString(TEST_HEX))
  );
});

add_task(function test_byteArrayRoundTrip() {
  Assert.ok(
    arraysEqual(
      TEST_BYTES,
      CommonUtils.stringToByteArray(CommonUtils.byteArrayToString(TEST_BYTES))
    )
  );
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
