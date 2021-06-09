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
    /* REQUIRED_CHARACTER_CLASSES */

    { length: 3 }
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
  let password = PasswordGenerator.generatePassword({ length: 5 });
  info(password);
  equal(password.length, 5, "Check length is correct");

  password = PasswordGenerator.generatePassword({ length: 2 });
  equal(password.length, 3, "Minimum generated length is 3");

  password = PasswordGenerator.generatePassword({ length: Math.pow(2, 8) });
  equal(
    password.length,
    Math.pow(2, 8) - 1,
    "Maximum generated length is Math.pow(2, 8) - 1 "
  );

  ok(
    password.match(/^[a-km-np-zA-HJ-NP-Z2-9]+$/),
    "All characters should be in the expected set"
  );
});

add_task(async function test_generatePassword_defaultLength() {
  let password = PasswordGenerator.generatePassword({});
  info(password);
  equal(password.length, 15, "Check default length is correct");
  ok(
    password.match(/^[a-km-np-zA-HJ-NP-Z2-9]{15}$/),
    "All characters should be in the expected set"
  );
});

add_task(
  async function test_generatePassword_immutableDefaultRequiredClasses() {
    // We need to escape our special characters since some of them
    // have special meaning in regex.
    let specialCharacters = PasswordGenerator._getSpecialCharacters();
    let escapedSpecialCharacters = specialCharacters.replace(
      /[.*\-+?^${}()|[\]\\]/g,
      "\\$&"
    );
    specialCharacters = new RegExp(`[${escapedSpecialCharacters}]`);
    let rules = new Map();
    rules.set("required", ["special"]);
    let password = PasswordGenerator.generatePassword({ rules });
    equal(password.length, 15, "Check default length is correct");
    ok(
      password.match(specialCharacters),
      "Password should include special character."
    );
    let allCharacters = new RegExp(
      `[a-km-np-zA-HJ-NP-Z2-9 ${escapedSpecialCharacters}]{15}`
    );
    ok(
      password.match(allCharacters),
      "All characters should be in the expected set"
    );
    password = PasswordGenerator.generatePassword({});
    equal(password.length, 15, "Check default length is correct");
    ok(
      !password.match(specialCharacters),
      "Password should not include special character."
    );
    ok(
      password.match(/^[a-km-np-zA-HJ-NP-Z2-9]{15}$/),
      "All characters, minus special characters, should be in the expected set"
    );
  }
);
