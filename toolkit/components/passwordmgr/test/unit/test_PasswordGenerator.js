"use strict";

const { PasswordGenerator } = ChromeUtils.importESModule(
  "resource://gre/modules/PasswordGenerator.sys.mjs"
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
  Assert.ok(
    Number.isSafeInteger(PasswordGenerator._randomUInt8Index(255)),
    "Check integer returned"
  );
});

add_task(async function test_generatePassword_classes() {
  let password = PasswordGenerator.generatePassword(
    /* REQUIRED_CHARACTER_CLASSES */

    { length: 4 }
  );
  info(password);
  equal(password.length, 4, "Check length is correct");
  Assert.ok(
    password.match(/[a-km-np-z]/),
    "Minimal password should include at least one lowercase character"
  );
  Assert.ok(
    password.match(/[A-HJ-NP-Z]/),
    "Minimal password should include at least one uppercase character"
  );
  Assert.ok(
    password.match(/[2-9]/),
    "Minimal password should include at least one digit"
  );
  Assert.ok(
    password.match(/[-~!@#$%^&*_+=)}:;"'>,.?\]]/),
    "Minimal password should include at least one special character"
  );
  Assert.ok(
    password.match(/^[a-km-np-zA-HJ-NP-Z2-9-~!@#$%^&*_+=)}:;"'>,.?\]]+$/),
    "All characters should be in the expected set"
  );
});

add_task(async function test_generatePassword_length() {
  let password = PasswordGenerator.generatePassword({ length: 5 });
  info(password);
  equal(password.length, 5, "Check length is correct");

  password = PasswordGenerator.generatePassword({ length: 3 });
  equal(password.length, 4, "Minimum generated length is 4");

  password = PasswordGenerator.generatePassword({ length: Math.pow(2, 8) });
  equal(
    password.length,
    Math.pow(2, 8) - 1,
    "Maximum generated length is Math.pow(2, 8) - 1 "
  );

  Assert.ok(
    password.match(/^[a-km-np-zA-HJ-NP-Z2-9-~!@#$%^&*_+=)}:;"'>,.?\]]+$/),
    "All characters should be in the expected set"
  );
});

add_task(async function test_generatePassword_defaultLength() {
  let password = PasswordGenerator.generatePassword({});
  info(password);
  equal(password.length, 15, "Check default length is correct");
  Assert.ok(
    password.match(/^[a-km-np-zA-HJ-NP-Z2-9-~!@#$%^&*_+=)}:;"'>,.?\]]{15}$/),
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
    Assert.ok(
      password.match(specialCharacters),
      "Password should include special character."
    );
    let allCharacters = new RegExp(
      `[a-km-np-zA-HJ-NP-Z2-9 ${escapedSpecialCharacters}]{15}`
    );
    Assert.ok(
      password.match(allCharacters),
      "All characters should be in the expected set"
    );
    password = PasswordGenerator.generatePassword({});
    equal(password.length, 15, "Check default length is correct");
    Assert.ok(
      password.match(specialCharacters),
      "Password should include special character."
    );
    Assert.ok(
      password.match(/^[a-km-np-zA-HJ-NP-Z2-9-~!@#$%^&*_+=)}:;"'>,.?\]]{15}$/),
      "All characters, minus special characters, should be in the expected set"
    );
  }
);
