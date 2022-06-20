/**
 * Test PasswordRulesManager.generatePassword()
 */

"use strict";
const { PasswordGenerator } = ChromeUtils.import(
  "resource://gre/modules/PasswordGenerator.jsm"
);
const { PasswordRulesManagerParent } = ChromeUtils.import(
  "resource://gre/modules/PasswordRulesManager.jsm"
);
const { PasswordRulesParser } = ChromeUtils.import(
  "resource://gre/modules/PasswordRulesParser.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

const IMPROVED_RULES_COLLECTION = "password-rules";

function getRulesForRecord(records, baseOrigin) {
  let rules;
  for (let record of records) {
    if (record.Domain === baseOrigin) {
      rules = record["password-rules"];
      break;
    }
  }
  return rules;
}

add_task(async function test_verify_password_rules() {
  const testCases = [
    { maxlength: 12 },
    { minlength: 4, maxlength: 32 },
    { required: ["lower"] },
    { required: ["upper"] },
    { required: ["digit"] },
    { required: ["special"] },
    { required: ["*", "$", "@", "_", "B", "Q"] },
    { required: ["lower", "upper", "special"] },
    { "max-consecutive": 2 },
    {
      minlength: 8,
      required: [
        "digit",
        [
          "-",
          " ",
          "!",
          '"',
          "#",
          "$",
          "&",
          "'",
          "(",
          ")",
          "*",
          "+",
          ",",
          ".",
          ":",
          ";",
          "<",
          "=",
          ">",
          "?",
          "@",
          "[",
          "^",
          "_",
          "`",
          "{",
          "|",
          "}",
          "~",
          "]",
        ],
      ],
    },
    { minlength: 8, maxlength: 16, required: ["lower", "upper", "digit"] },
    {
      minlength: 8,
      maxlength: 20,
      required: ["lower", "upper", "digit"],
      "max-consecutive": 2,
    },
  ];

  for (let test of testCases) {
    let mapOfRules = new Map();
    let rules = ``;
    for (let testRules in test) {
      mapOfRules.set(testRules, test[testRules]);
      if (test[testRules] === "required" && test[testRules].includes("*$")) {
        rules += `${testRules}: ${test[testRules].join("")}`;
      } else {
        rules += `${testRules}: ${test[testRules]};`;
      }
    }
    let generatedPassword = PasswordGenerator.generatePassword({
      rules: mapOfRules,
    });
    verifyPassword(rules, generatedPassword);
  }
});

/**
 * Note: We do not test the "allowed" property in these tests.
 * This is because a password can still be valid even if there is not a character from
 * the "allowed" list.
 * If a character, or character class, is required, then it should be marked as such.
 * */

add_task(async function test_generatePassword_many_rules() {
  // Force password generation to be enabled.
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);
  Services.prefs.setBoolPref("signon.improvedPasswordRules.enabled", true);
  // TEST_ORIGIN emulates the browsingContext.currentWindowGlobal.documentURI variable in LoginManagerParent
  // and so it should always be a correctly formed URI when working with
  // the PasswordRulesParser and PasswordRulesManager modules
  const TEST_ORIGIN = Services.io.newURI("https://example.com");

  // TEST_BASE_ORIGIN is how each domain is stored in RemoteSettings, and so
  // we need this in order to parse out the particular password rules we're verifying
  const TEST_BASE_ORIGIN = "example.com";
  await LoginTestUtils.remoteSettings.setupImprovedPasswordRules();
  const records = await RemoteSettings(IMPROVED_RULES_COLLECTION).get();

  let rules = getRulesForRecord(records, TEST_BASE_ORIGIN);

  rules = PasswordRulesParser.parsePasswordRules(rules);
  ok(rules.length, "Rules should exist after parsing");

  let PRMP = new PasswordRulesManagerParent();
  ok(PRMP.generatePassword, "PRMP.generatePassword exists");

  let generatedPassword = await PRMP.generatePassword(TEST_ORIGIN);
  ok(generatedPassword, "A password was generated");

  verifyPassword(rules, generatedPassword);

  await LoginTestUtils.remoteSettings.cleanImprovedPasswordRules();
});

add_task(async function test_generatePassword_all_characters_allowed() {
  // Force password generation to be enabled.
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);
  Services.prefs.setBoolPref("signon.improvedPasswordRules.enabled", true);
  // TEST_ORIGIN emulates the browsingContext.currentWindowGlobal.documentURI variable in LoginManagerParent
  // and so it should always be a correctly formed URI when working with
  // the PasswordRulesParser and PasswordRulesManager modules
  const TEST_ORIGIN = Services.io.newURI("https://example.com");

  // TEST_BASE_ORIGIN is how each domain is stored in RemoteSettings, and so
  // we need this in order to parse out the particular password rules we're verifying
  const TEST_BASE_ORIGIN = "example.com";
  const TEST_RULES = "minlength: 6; maxlength: 12;";
  await LoginTestUtils.remoteSettings.setupImprovedPasswordRules(
    TEST_BASE_ORIGIN,
    TEST_RULES
  );
  const records = await RemoteSettings(IMPROVED_RULES_COLLECTION).get();

  let rules = getRulesForRecord(records, TEST_BASE_ORIGIN);

  rules = PasswordRulesParser.parsePasswordRules(rules);
  ok(rules.length, "Rules should exist after parsing");

  let PRMP = new PasswordRulesManagerParent();
  ok(PRMP.generatePassword, "PRMP.generatePassword exists");

  let generatedPassword = await PRMP.generatePassword(TEST_ORIGIN);
  ok(generatedPassword, "A password was generated");

  verifyPassword(rules, generatedPassword);

  await LoginTestUtils.remoteSettings.cleanImprovedPasswordRules();
});

add_task(async function test_generatePassword_required_special_character() {
  // Force password generation to be enabled.
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);
  Services.prefs.setBoolPref("signon.improvedPasswordRules.enabled", true);
  // TEST_ORIGIN emulates the browsingContext.currentWindowGlobal.documentURI variable in LoginManagerParent
  // and so it should always be a correctly formed URI when working with
  // the PasswordRulesParser and PasswordRulesManager modules
  const TEST_ORIGIN = Services.io.newURI("https://example.com");

  // TEST_BASE_ORIGIN is how each domain is stored in RemoteSettings, and so
  // we need this in order to parse out the particular password rules we're verifying
  const TEST_BASE_ORIGIN = "example.com";
  const TEST_RULES = "required: special";
  await LoginTestUtils.remoteSettings.setupImprovedPasswordRules(
    TEST_BASE_ORIGIN,
    TEST_RULES
  );
  const records = await RemoteSettings(IMPROVED_RULES_COLLECTION).get();

  let rules = getRulesForRecord(records, TEST_BASE_ORIGIN);

  rules = PasswordRulesParser.parsePasswordRules(rules);
  ok(rules.length, "Rules should exist after parsing");

  let PRMP = new PasswordRulesManagerParent();
  ok(PRMP.generatePassword, "PRMP.generatePassword exists");

  let generatedPassword = await PRMP.generatePassword(TEST_ORIGIN);
  ok(generatedPassword, "A password was generated");

  verifyPassword(TEST_RULES, generatedPassword);
});

add_task(
  async function test_generatePassword_with_arbitrary_required_characters() {
    // Force password generation to be enabled.
    Services.prefs.setBoolPref("signon.generation.available", true);
    Services.prefs.setBoolPref("signon.generation.enabled", true);
    Services.prefs.setBoolPref("signon.improvedPasswordRules.enabled", true);
    // TEST_ORIGIN emulates the browsingContext.currentWindowGlobal.documentURI variable in LoginManagerParent
    // and so it should always be a correctly formed URI when working with
    // the PasswordRulesParser and PasswordRulesManager modules
    const TEST_ORIGIN = Services.io.newURI("https://example.com");

    // TEST_BASE_ORIGIN is how each domain is stored in RemoteSettings, and so
    // we need this in order to parse out the particular password rules we're verifying
    const TEST_BASE_ORIGIN = "example.com";
    const REQUIRED_ARBITRARY_CHARACTERS = "!#$@*()_+=";
    // We use an extremely long password to ensure there are no invalid characters generated in the password.
    // This ensures we exhaust all of "allRequiredCharacters" in PasswordGenerator.jsm.
    // Otherwise, there's a small chance a "," may have been added to "allRequiredCharacters"
    // which will generate an invalid password in this case.
    const TEST_RULES = `required: [${REQUIRED_ARBITRARY_CHARACTERS}], upper, lower; maxlength: 255; minlength: 255;`;
    await LoginTestUtils.remoteSettings.setupImprovedPasswordRules(
      TEST_BASE_ORIGIN,
      TEST_RULES
    );
    const records = await RemoteSettings(IMPROVED_RULES_COLLECTION).get();

    let rules = getRulesForRecord(records, TEST_BASE_ORIGIN);

    rules = PasswordRulesParser.parsePasswordRules(rules);
    ok(rules.length, "Rules should exist after parsing");

    let PRMP = new PasswordRulesManagerParent();
    ok(PRMP.generatePassword, "PRMP.generatePassword exists");

    let generatedPassword = await PRMP.generatePassword(TEST_ORIGIN);
    generatedPassword = await PRMP.generatePassword(TEST_ORIGIN);
    ok(generatedPassword, "A password was generated");

    verifyPassword(TEST_RULES, generatedPassword);

    let specialCharacters = PasswordGenerator._getSpecialCharacters();
    let digits = PasswordGenerator._getDigits();
    // Additional verification for this password case since
    // we want to ensure no extra special characters and no digits are generated.
    let disallowedSpecialCharacters = "";
    for (let char of specialCharacters) {
      if (!REQUIRED_ARBITRARY_CHARACTERS.includes(char)) {
        disallowedSpecialCharacters += char;
      }
    }
    for (let char of disallowedSpecialCharacters) {
      ok(
        !generatedPassword.includes(char),
        "Password must not contain any disallowed special characters: " + char
      );
    }
    for (let char of digits) {
      ok(
        !generatedPassword.includes(char),
        "Password must not contain any digits: " + char
      );
    }
  }
);

// Checks the "www4.prepaid.bankofamerica.com" case to ensure the rules are found
add_task(async function test_generatePassword_subdomain_rule() {
  const testCases = [
    {
      uri: "https://www4.test.example.com",
      rulesDomain: "example.com",
      shouldApplyPWRule: true,
    },
    {
      uri: "https://test.example.com",
      rulesDomain: "example.com",
      shouldApplyPWRule: true,
    },
    {
      uri: "https://example.com",
      rulesDomain: "example.com",
      shouldApplyPWRule: true,
    },
    {
      uri: "https://www4.test.example.com",
      rulesDomain: "test.example.com",
      shouldApplyPWRule: true,
    },
    {
      uri: "https://test.example.com",
      rulesDomain: "test.example.com",
      shouldApplyPWRule: true,
    },
    {
      uri: "https://example.com",
      rulesDomain: "test.example.com",
      shouldApplyPWRule: false,
    },
    {
      uri: "https://evil.com",
      rulesDomain: "example.com",
      shouldApplyPWRule: false,
    },
    {
      uri: "https://evil.example.com",
      rulesDomain: "test.example.com",
      shouldApplyPWRule: false,
    },
    {
      uri: "https://test.example.com.cn",
      rulesDomain: "test.example.com",
      shouldApplyPWRule: false,
    },
    {
      uri: "https://eviltest.example.com",
      rulesDomain: "test.example.com",
      shouldApplyPWRule: false,
    },
  ];
  const TEST_RULES = "required: special; maxlength: 12;";

  for (let test of testCases) {
    await LoginTestUtils.remoteSettings.setupImprovedPasswordRules(
      test.rulesDomain,
      TEST_RULES
    );
    const TEST_ORIGIN = Services.io.newURI(test.uri);
    const TEST_BASE_ORIGIN = test.rulesDomain;
    const records = await RemoteSettings(IMPROVED_RULES_COLLECTION).get();

    let rules = getRulesForRecord(records, TEST_BASE_ORIGIN);

    rules = PasswordRulesParser.parsePasswordRules(rules);
    ok(rules.length, "Rules should exist after parsing");

    let PRMP = new PasswordRulesManagerParent();

    let generatedPassword = await PRMP.generatePassword(TEST_ORIGIN);
    ok(generatedPassword, "A password was generated for URI: " + test.uri);

    // If a rule should be applied, we verify the password has all the required classes in the generated password.
    if (test.shouldApplyPWRule) {
      verifyPassword(rules, generatedPassword);
    } else {
      // If a rule should not be applied, we verify that the generated password has no special characters
      // since our standard generation does not include special characters.

      const SPECIAL_CHARACTERS = PasswordGenerator._getSpecialCharacters();
      // We need to escape our special characters since some of them
      // have special meaning in regex.
      let escapedSpecialCharacters = SPECIAL_CHARACTERS.replace(
        /[.*\-+?^${}()|[\]\\]/g,
        "\\$&"
      );
      let checkSpecial = new RegExp(`[${escapedSpecialCharacters}]`);
      ok(!generatedPassword.match(checkSpecial));
    }
  }
});

add_task(async function test_improved_password_rules_telemetry() {
  // Force password generation to be enabled.
  Services.prefs.setBoolPref("signon.generation.available", true);
  Services.prefs.setBoolPref("signon.generation.enabled", true);
  Services.prefs.setBoolPref("signon.improvedPasswordRules.enabled", true);

  const IMPROVED_PASSWORD_GENERATION_HISTOGRAM =
    "PWMGR_NUM_IMPROVED_GENERATED_PASSWORDS";

  // Clear out the previous pings from this test
  let snapshot = TelemetryTestUtils.getAndClearHistogram(
    IMPROVED_PASSWORD_GENERATION_HISTOGRAM
  );

  // TEST_ORIGIN emulates the browsingContext.currentWindowGlobal.documentURI variable in LoginManagerParent
  // and so it should always be a correctly formed URI when working with
  // the PasswordRulesParser and PasswordRulesManager modules
  let TEST_ORIGIN = Services.io.newURI("https://example.com");
  await LoginTestUtils.remoteSettings.setupImprovedPasswordRules();

  let PRMP = new PasswordRulesManagerParent();

  // Generate a password with custom rules,
  // so we should send a ping to the custom rules bucket (position 1).
  let generatedPassword = await PRMP.generatePassword(TEST_ORIGIN);
  ok(generatedPassword, "A password was generated");

  TelemetryTestUtils.assertHistogram(snapshot, 1, 1);

  TEST_ORIGIN = Services.io.newURI("https://otherexample.com");
  // Generate a password with default rules,
  // so we should send a ping to the default rules bucket (position 0).
  snapshot = TelemetryTestUtils.getAndClearHistogram(
    IMPROVED_PASSWORD_GENERATION_HISTOGRAM
  );
  generatedPassword = await PRMP.generatePassword(TEST_ORIGIN);
  ok(generatedPassword, "A password was generated");

  TelemetryTestUtils.assertHistogram(snapshot, 0, 1);
});

function checkCharacters(password, _characters) {
  let containsCharacters = false;
  let testString = _characters.join("");
  for (let character of password) {
    containsCharacters = testString.includes(character);
    if (containsCharacters) {
      return containsCharacters;
    }
  }
  return containsCharacters;
}

function checkConsecutiveCharacters(generatePassword, value) {
  let findMaximumRepeating = str => {
    let max = 0;
    for (let start = 0, end = 1; end < str.length; ) {
      if (str[end] === str[start]) {
        if (max < end - start + 1) {
          max = end - start + 1;
          if (max > value) {
            return max;
          }
        }
        end++;
      } else {
        start = end++;
      }
    }
    return max;
  };
  let consecutiveCharacters = findMaximumRepeating(generatePassword);
  if (consecutiveCharacters <= value) {
    return true;
  }
  return false;
}

function verifyPassword(rules, generatedPassword) {
  const UPPER_CASE_ALPHA = PasswordGenerator._getUpperCaseCharacters();
  const LOWER_CASE_ALPHA = PasswordGenerator._getLowerCaseCharacters();
  const DIGITS = PasswordGenerator._getDigits();
  const SPECIAL_CHARACTERS = PasswordGenerator._getSpecialCharacters();
  for (let rule of rules) {
    let { _name, value } = rule;
    if (_name === "required") {
      for (let required of value) {
        if (required._name === "upper") {
          let _checkUppercase = new RegExp(`[${UPPER_CASE_ALPHA}]`);
          ok(
            generatedPassword.match(_checkUppercase),
            "Password must include upper case letter"
          );
        } else if (required._name === "lower") {
          let _checkLowercase = new RegExp(`[${LOWER_CASE_ALPHA}]`);
          ok(
            generatedPassword.match(_checkLowercase),
            "Password must include lower case letter"
          );
        } else if (required._name === "digit") {
          let _checkDigits = new RegExp(`[${DIGITS}]`);
          ok(
            generatedPassword.match(_checkDigits),
            "Password must include digits"
          );
          generatedPassword.match(_checkDigits);
        } else if (required._name === "special") {
          // We need to escape our special characters since some of them
          // have special meaning in regex.
          let escapedSpecialCharacters = SPECIAL_CHARACTERS.replace(
            /[.*\-+?^${}()|[\]\\]/g,
            "\\$&"
          );
          let _checkSpecial = new RegExp(`[${escapedSpecialCharacters}]`);
          ok(
            generatedPassword.match(_checkSpecial),
            "Password must include special character"
          );
        } else {
          // Nested destructing of the value object in the characters case
          let [{ _characters }] = value;

          // We can't use regex to do a quick check here since the
          // required characters could be characters that need to be escaped
          // in order for the regex to work properly ([]"^...etc)
          ok(
            checkCharacters(generatedPassword, _characters),
            `Password must contain one of the following characters: ${_characters}`
          );
        }
      }
    } else if (_name === "minlength") {
      ok(
        generatedPassword.length >= value,
        `Password should have a minimum length of ${value}`
      );
    } else if (_name === "maxlength") {
      ok(
        generatedPassword.length <= value,
        `Password should have a maximum length of ${value}`
      );
    } else if (_name === "max-consecutive") {
      ok(
        checkConsecutiveCharacters(generatedPassword, value),
        `Password must not contain more than ${value} consecutive characters`
      );
    }
  }
}
