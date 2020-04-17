/**
 * Test for LoginManagerChild._getFormFields.
 */

"use strict";

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const { LoginFormFactory } = ChromeUtils.import(
  "resource://gre/modules/LoginFormFactory.jsm"
);
const LMCBackstagePass = ChromeUtils.import(
  "resource://gre/modules/LoginManagerChild.jsm",
  null
);
const { LoginManagerChild } = LMCBackstagePass;
const TESTCASES = [
  {
    description: "1 password field outside of a <form>",
    document: `<input id="pw1" type=password>`,
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description: "1 text field outside of a <form> without a password field",
    document: `<input id="un1">`,
    returnedFieldIDs: [null, null, null],
    skipEmptyFields: undefined,
  },
  {
    description: "1 username & password field outside of a <form>",
    document: `<input id="un1">
      <input id="pw1" type=password>`,
    returnedFieldIDs: ["un1", "pw1", null],
    skipEmptyFields: undefined,
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
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description: "1 username & password field in a <form>",
    document: `<form>
      <input id="un1">
      <input id="pw1" type=password>
      </form>`,
    returnedFieldIDs: ["un1", "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description: "5 empty password fields outside of a <form>",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password>
      <input id="pw3" type=password>
      <input id="pw4" type=password>
      <input id="pw5" type=password>`,
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description: "6 empty password fields outside of a <form>",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password>
      <input id="pw3" type=password>
      <input id="pw4" type=password>
      <input id="pw5" type=password>
      <input id="pw6" type=password>`,
    returnedFieldIDs: [null, null, null],
    skipEmptyFields: undefined,
  },
  {
    description:
      "4 password fields outside of a <form> (1 empty, 3 full) with skipEmpty",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password value="pass2">
      <input id="pw3" type=password value="pass3">
      <input id="pw4" type=password value="pass4">`,
    returnedFieldIDs: [null, null, null],
    skipEmptyFields: true,
  },
  {
    description: "Form with 1 password field",
    document: `<form><input id="pw1" type=password></form>`,
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description: "Form with 2 password fields",
    document: `<form><input id="pw1" type=password><input id='pw2' type=password></form>`,
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description: "1 password field in a form, 1 outside (not processed)",
    document: `<form><input id="pw1" type=password></form><input id="pw2" type=password>`,
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description:
      "1 password field in a form, 1 text field outside (not processed)",
    document: `<form><input id="pw1" type=password></form><input>`,
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description:
      "1 text field in a form, 1 password field outside (not processed)",
    document: `<form><input></form><input id="pw1" type=password>`,
    returnedFieldIDs: [null, null, null],
    skipEmptyFields: undefined,
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form",
    document: `<input id="pw1" type=password><input id="pw2" type=password form='form1'>
      <form id="form1"></form>`,
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: undefined,
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form + skipEmpty",
    document: `<input id="pw1" type=password><input id="pw2" type=password form="form1">
      <form id="form1"></form>`,
    returnedFieldIDs: [null, null, null],
    skipEmptyFields: true,
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form + skipEmpty with 1 empty",
    document: `<input id="pw1" type=password value="pass1"><input id="pw2" type=password form="form1">
      <form id="form1"></form>`,
    returnedFieldIDs: [null, "pw1", null],
    skipEmptyFields: true,
  },
];

for (let tc of TESTCASES) {
  info("Sanity checking the testcase: " + tc.description);

  (function() {
    let testcase = tc;
    add_task(async function() {
      info("Starting testcase: " + testcase.description);
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

      let actual = new LoginManagerChild()._getFormFields(
        formLike,
        testcase.skipEmptyFields,
        new Set()
      );

      Assert.strictEqual(
        testcase.returnedFieldIDs.length,
        3,
        "_getFormFields returns 3 elements"
      );

      for (let i = 0; i < testcase.returnedFieldIDs.length; i++) {
        let expectedID = testcase.returnedFieldIDs[i];
        if (expectedID === null) {
          Assert.strictEqual(
            actual[i],
            expectedID,
            "Check returned field " + i + " is null"
          );
        } else {
          Assert.strictEqual(
            actual[i].id,
            expectedID,
            "Check returned field " + i + " ID"
          );
        }
      }
    });
  })();
}
