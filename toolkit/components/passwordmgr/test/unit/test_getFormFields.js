/**
 * Test for LoginFormState._getFormFields.
 */

"use strict";

const { LoginFormFactory } = ChromeUtils.import(
  "resource://gre/modules/LoginFormFactory.jsm"
);

const { LoginManagerChild } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerChild.jsm"
);

const TESTENVIRONMENTS = {
  filledPW1WithGeneratedPassword: {
    generatedPWFieldSelectors: ["#pw1"],
  },
};

const TESTCASES = [
  {
    description: "1 password field outside of a <form>",
    document: `<input id="pw1" type=password>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "1 text field outside of a <form> without a password field",
    document: `<input id="un1">`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: null,
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    // there is no password field to fill, so no sense testing with gen. passwords
    extraTestEnvironments: [],
    extraTestPreferences: [],
  },
  {
    description: "1 username & password field outside of a <form>",
    document: `<input id="un1">
      <input id="pw1" type=password>`,
    returnedFieldIDs: {
      usernameField: "un1",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    beforeGetFunction(doc, formLike) {
      // Access the formLike.elements lazy getter to have it cached.
      Assert.equal(
        formLike.elements.length,
        2,
        "Check initial elements length"
      );
      doc.getElementById("un1").remove();
    },
    description: "1 username & password field outside of a <form>, un1 removed",
    document: `<input id="un1">
      <input id="pw1" type=password>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "1 username & password field in a <form>",
    document: `<form>
      <input id="un1">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: {
      usernameField: "un1",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "5 empty password fields outside of a <form>",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password>
      <input id="pw3" type=password>
      <input id="pw4" type=password>
      <input id="pw5" type=password>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "6 empty password fields outside of a <form>",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password>
      <input id="pw3" type=password>
      <input id="pw4" type=password>
      <input id="pw5" type=password>
      <input id="pw6" type=password>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: null,
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "4 password fields outside of a <form> (1 empty, 3 full) with skipEmpty",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password value="pass2">
      <input id="pw3" type=password value="pass3">
      <input id="pw4" type=password value="pass4">`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: null,
      oldPasswordField: null,
    },
    skipEmptyFields: true,
    // This test assumes that pw1 has not been filled, so don't test prefilling it
    extraTestEnvironments: [],
    extraTestPreferences: [],
  },
  {
    description: "Form with 1 password field",
    document: `<form><input id="pw1" type=password></form>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "Form with 2 password fields",
    document: `<form><input id="pw1" type=password><input id='pw2' type=password></form>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "1 password field in a form, 1 outside (not processed)",
    document: `<form><input id="pw1" type=password></form><input id="pw2" type=password>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "1 password field in a form, 1 text field outside (not processed)",
    document: `<form><input id="pw1" type=password></form><input>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "1 text field in a form, 1 password field outside (not processed)",
    document: `<form><input></form><input id="pw1" type=password>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: null,
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form",
    document: `<input id="pw1" type=password><input id="pw2" type=password form='form1'>
      <form id="form1"></form>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form + skipEmpty",
    document: `<input id="pw1" type=password><input id="pw2" type=password form="form1">
      <form id="form1"></form>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: null,
      oldPasswordField: null,
    },
    skipEmptyFields: true,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form + skipEmpty with 1 empty",
    document: `<input id="pw1" type=password value="pass1"><input id="pw2" type=password form="form1">
      <form id="form1"></form>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: true,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "3 password fields, 2nd and 3rd are filled with generated passwords",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password value="pass2">
      <input id="pw3" type=password value="pass3">`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: "pw2",
      confirmPasswordField: "pw3",
      oldPasswordField: "pw1",
    },
    skipEmptyFields: undefined,
    generatedPWFieldSelectors: ["#pw2", "#pw3"],
    // this test doesn't make sense to run with different filled generated password values
    extraTestEnvironments: [],
    extraTestPreferences: [],
  },
  // begin of getusername heuristic tests
  {
    description: "multiple non-username like input fields in a <form>",
    document: `<form>
      <input id="un1">
      <input id="un2">
      <input id="un3">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: {
      usernameField: "un3",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "1 username input and multiple non-username like input in a <form>",
    document: `<form>
      <input id="un1">
      <input id="un2" autocomplete="username">
      <input id="un3">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: {
      usernameField: "un2",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "1 email input and multiple non-username like input in a <form>",
    document: `<form>
      <input id="un1">
      <input id="un2" autocomplete="email">
      <input id="un3">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: {
      usernameField: "un2",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "1 username & 1 email field, the email field is more close to the password",
    document: `<form>
      <input id="un1" autocomplete="username">
      <input id="un2" autocomplete="email">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: {
      usernameField: "un1",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description:
      "1 username and 1 email field, the username field is more close to the password",
    document: `<form>
      <input id="un1" autocomplete="email">
      <input id="un2" autocomplete="username">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: {
      usernameField: "un2",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "2 username fields in a <form>",
    document: `<form>
      <input id="un1" autocomplete="username">
      <input id="un2" autocomplete="username">
      <input id="un3">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: {
      usernameField: "un2",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "2 email fields in a <form>",
    document: `<form>
      <input id="un1" autocomplete="email">
      <input id="un2" autocomplete="email">
      <input id="un3">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: {
      usernameField: "un1",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  {
    description: "the password field precedes the username field",
    document: `<form>
      <input id="un1">
      <input id="pw1" type=password>
      <input id="un2" autocomplete="username">
      </form>`,
    returnedFieldIDs: {
      usernameField: "un1",
      newPasswordField: "pw1",
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [TESTENVIRONMENTS.filledPW1WithGeneratedPassword],
    extraTestPreferences: [],
  },
  // end of getusername heuristic tests
  {
    description: "1 username field in a <form>",
    document: `<form>
      <input id="un1" autocomplete="username">
      </form>`,
    returnedFieldIDs: {
      usernameField: "un1",
      newPasswordField: null,
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [],
    extraTestPreferences: [],
  },
  {
    description: "1 input field in a <form>",
    document: `<form>
      <input id="un1"">
      </form>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: null,
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [],
    extraTestPreferences: [],
  },
  {
    description: "1 username field in a <form> with usernameOnlyForm pref off",
    document: `<form>
      <input id="un1" autocomplete="username">
      </form>`,
    returnedFieldIDs: {
      usernameField: null,
      newPasswordField: null,
      oldPasswordField: null,
    },
    skipEmptyFields: undefined,
    extraTestEnvironments: [],
    extraTestPreferences: [["signon.usernameOnlyForm.enabled", false]],
  },
];

const TEST_ENVIRONMENT_CASES = TESTCASES.flatMap(tc => {
  let arr = [tc];
  // also run this test case with this different state
  for (let env of tc.extraTestEnvironments) {
    arr.push({
      ...tc,
      ...env,
    });
  }
  return arr;
});

function _setPrefs() {
  Services.prefs.setBoolPref("signon.usernameOnlyForm.enabled", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.usernameOnlyForm.enabled");
  });
}

_setPrefs();

for (let tc of TEST_ENVIRONMENT_CASES) {
  info("Sanity checking the testcase: " + tc.description);

  (function() {
    let testcase = tc;
    add_task(async function() {
      info("Starting testcase: " + testcase.description);

      for (let pref of testcase.extraTestPreferences) {
        Services.prefs.setBoolPref(pref[0], pref[1]);
      }

      info("Document string: " + testcase.document);
      let document = MockDocument.createTestDocument(
        "http://localhost:8080/test/",
        testcase.document
      );

      let input = document.querySelector("input");
      MockDocument.mockOwnerDocumentProperty(
        input,
        document,
        "http://localhost:8080/test/"
      );

      let formLike = LoginFormFactory.createFromField(input);

      if (testcase.beforeGetFunction) {
        await testcase.beforeGetFunction(document, formLike);
      }

      let lmc = new LoginManagerChild();
      let loginFormState = lmc.stateForDocument(formLike.ownerDocument);
      loginFormState.generatedPasswordFields = _generateDocStateFromTestCase(
        testcase,
        document
      );

      let actual = loginFormState._getFormFields(
        formLike,
        testcase.skipEmptyFields,
        new Set()
      );

      [
        "usernameField",
        "newPasswordField",
        "oldPasswordField",
        "confirmPasswordField",
      ].forEach(fieldName => {
        Assert.ok(
          fieldName in actual,
          "_getFormFields return value includes " + fieldName
        );
      });
      for (let key of Object.keys(testcase.returnedFieldIDs)) {
        let expectedID = testcase.returnedFieldIDs[key];
        if (expectedID === null) {
          Assert.strictEqual(
            actual[key],
            expectedID,
            "Check returned field " + key + " is null"
          );
        } else {
          Assert.strictEqual(
            actual[key].id,
            expectedID,
            "Check returned field " + key + " ID"
          );
        }
      }

      for (let pref of tc.extraTestPreferences) {
        Services.prefs.clearUserPref(pref[0]);
      }
    });
  })();
}

function _generateDocStateFromTestCase(stateProperties, document) {
  // prepopulate the document form state LMC holds with
  // any generated password fields defined in this testcase
  let generatedPasswordFields = new Set();
  info(
    "stateProperties has generatedPWFieldSelectors: " +
      stateProperties.generatedPWFieldSelectors?.join(", ")
  );

  if (stateProperties.generatedPWFieldSelectors?.length) {
    stateProperties.generatedPWFieldSelectors.forEach(sel => {
      let field = document.querySelector(sel);
      if (field) {
        generatedPasswordFields.add(field);
      } else {
        info(`No password field: ${sel} found in this document`);
      }
    });
  }
  return generatedPasswordFields;
}
