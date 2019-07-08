"use strict";

const { PasswordGenerator } = ChromeUtils.import(
  "resource://gre/modules/PasswordGenerator.jsm"
);

add_task(async function test_shuffleString() {
  let original = "1234567890";
  let shuffled = PasswordGenerator._shuffleString(original);
  notEqual(original, shuffled, "String should have been shuffled");
});

add_task(async function test_randomUInt8Index() {
  throws(
    () => PasswordGenerator._randomUInt8Index(256),
    /uint8/,
    "Should throw for larger than uint8"
  );
  ok(
    Number.isSafeInteger(PasswordGenerator._randomUInt8Index(255)),
    "Check integer returned"
  );
});

add_task(async function test_generatePassword_classes() {
  let password = PasswordGenerator.generatePassword(
    /* REQUIRED_CHARACTER_CLASSES */ 3
  );
  info(password);
  equal(password.length, 3, "Check length is correct");
  ok(
    password.match(/[a-km-np-z]/),
    "Minimal password should include at least one lowercase character"
  );
  ok(
    password.match(/[A-HJ-NP-Z]/),
    "Minimal password should include at least one uppercase character"
  );
  ok(
    password.match(/[2-9]/),
    "Minimal password should include at least one digit"
  );
  ok(
    password.match(/^[a-km-np-zA-HJ-NP-Z2-9]+$/),
    "All characters should be in the expected set"
  );
});

add_task(async function test_generatePassword_length() {
  let password = PasswordGenerator.generatePassword(5);
  info(password);
  equal(password.length, 5, "Check length is correct");

  throws(
    () => PasswordGenerator.generatePassword(2),
    /too short/i,
    "Throws if too short"
  );
  throws(
    () => PasswordGenerator.generatePassword(Math.pow(2, 8)),
    /too long/i,
    "Throws if too long"
  );
  ok(
    password.match(/^[a-km-np-zA-HJ-NP-Z2-9]+$/),
    "All characters should be in the expected set"
  );
});

add_task(async function test_generatePassword_defaultLength() {
  let password = PasswordGenerator.generatePassword();
  info(password);
  equal(password.length, 15, "Check default length is correct");
  ok(
    password.match(/^[a-km-np-zA-HJ-NP-Z2-9]{15}$/),
    "All characters should be in the expected set"
  );
});
