/**
 * Test for LoginFormState._getPasswordFields using LoginFormFactory.
 */

/* globals todo_check_eq */
"use strict";

const { LoginFormFactory } = ChromeUtils.import(
  "resource://gre/modules/LoginFormFactory.jsm"
);
const { LoginFormState } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerChild.jsm"
);
const TESTCASES = [
  {
    description: "Empty document",
    document: ``,
    returnedFieldIDsByFormLike: [],
    minPasswordLength: undefined,
  },
  {
    description: "Non-password input with no <form> present",
    document: `<input>`,
    // Only the IDs of password fields should be in this array
    returnedFieldIDsByFormLike: [[]],
    minPasswordLength: undefined,
  },
  {
    description: "1 password field outside of a <form>",
    document: `<input id="pw1" type=password>`,
    returnedFieldIDsByFormLike: [["pw1"]],
    minPasswordLength: undefined,
  },
  {
    description: "5 empty password fields outside of a <form>",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password>
      <input id="pw3" type=password>
      <input id="pw4" type=password>
      <input id="pw5" type=password>`,
    returnedFieldIDsByFormLike: [["pw1", "pw2", "pw3", "pw4", "pw5"]],
    minPasswordLength: undefined,
  },
  {
    description: "6 empty password fields outside of a <form>",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password>
      <input id="pw3" type=password>
      <input id="pw4" type=password>
      <input id="pw5" type=password>
      <input id="pw6" type=password>`,
    returnedFieldIDsByFormLike: [[]],
    minPasswordLength: undefined,
  },
  {
    description:
      "4 password fields outside of a <form> (1 empty, 3 full) with minPasswordLength=2",
    document: `<input id="pw1" type=password>
      <input id="pw2" type=password value="pass2">
      <input id="pw3" type=password value="pass3">
      <input id="pw4" type=password value="pass4">`,
    returnedFieldIDsByFormLike: [["pw2", "pw3", "pw4"]],
    minPasswordLength: 2,
  },
  {
    description: "Form with 1 password field",
    document: `<form><input id="pw1" type=password></form>`,
    returnedFieldIDsByFormLike: [["pw1"]],
    minPasswordLength: undefined,
  },
  {
    description: "Form with 2 password fields",
    document: `<form><input id="pw1" type=password><input id='pw2' type=password></form>`,
    returnedFieldIDsByFormLike: [["pw1", "pw2"]],
    minPasswordLength: undefined,
  },
  {
    description: "1 password field in a form, 1 outside",
    document: `<form><input id="pw1" type=password></form><input id="pw2" type=password>`,
    returnedFieldIDsByFormLike: [["pw1"], ["pw2"]],
    minPasswordLength: undefined,
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form",
    document: `<input id="pw1" type=password><input id="pw2" type=password form='form1'>
      <form id="form1"></form>`,
    returnedFieldIDsByFormLike: [["pw1"], ["pw2"]],
    minPasswordLength: undefined,
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form + minPasswordLength",
    document: `<input id="pw1" type=password><input id="pw2" type=password form="form1">
      <form id="form1"></form>`,
    returnedFieldIDsByFormLike: [[], []],
    minPasswordLength: 2,
  },
  {
    description: "minPasswordLength should also skip white-space only fields",
    /* eslint-disable no-tabs */
    document: `<input id="pw-space" type=password value=" ">
               <input id="pw-tab" type=password value="	">
               <input id="pw-newline" type=password form="form1" value="
                                                                        ">
      <form id="form1"></form>`,
    /* eslint-enable no-tabs */
    returnedFieldIDsByFormLike: [[], []],
    minPasswordLength: 2,
  },
  {
    description: "minPasswordLength should skip too-short field values",
    document: `<form>
               <input id="pw-empty" type=password>
               <input id="pw-tooshort" type=password value="p">
               <input id="pw" type=password value="pz">
               </form>`,
    returnedFieldIDsByFormLike: [["pw"]],
    minPasswordLength: 2,
  },
  {
    description: "minPasswordLength should allow matching-length field values",
    document: `<form>
               <input id="pw-empty" type=password>
               <input id="pw-matchlen" type=password value="pz">
               <input id="pw" type=password value="pazz">
               </form>`,
    returnedFieldIDsByFormLike: [["pw-matchlen", "pw"]],
    minPasswordLength: 2,
  },
  {
    description:
      "2 password fields outside of a <form> with 1 linked via @form + minPasswordLength with 1 empty",
    document: `<input id="pw1" type=password value=" pass1 "><input id="pw2" type=password form="form1">
      <form id="form1"></form>`,
    returnedFieldIDsByFormLike: [["pw1"], []],
    minPasswordLength: 2,
    fieldOverrideRecipe: {
      // Ensure a recipe without `notPasswordSelector` doesn't cause a problem.
      hosts: ["localhost:8080"],
    },
  },
  {
    description:
      "3 password fields outside of a <form> with 1 linked via @form + minPasswordLength",
    document: `<input id="pw1" type=password value="pass1"><input id="pw2" type=password form="form1" value="pass2"><input id="pw3" type=password value="pass3">
      <form id="form1"><input id="pw4" type=password></form>`,
    returnedFieldIDsByFormLike: [["pw3"], ["pw2"]],
    minPasswordLength: 2,
    fieldOverrideRecipe: {
      hosts: ["localhost:8080"],
      notPasswordSelector: "#pw1",
    },
  },
  {
    beforeGetFunction(doc) {
      doc.getElementById("pw1").remove();
    },
    description:
      "1 password field outside of a <form> which gets removed/disconnected",
    document: `<input id="pw1" type=password>`,
    returnedFieldIDsByFormLike: [[]],
    minPasswordLength: undefined,
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

      let mapRootElementToFormLike = new Map();
      for (let input of document.querySelectorAll("input")) {
        let formLike = LoginFormFactory.createFromField(input);
        let existingFormLike = mapRootElementToFormLike.get(
          formLike.rootElement
        );
        if (!existingFormLike) {
          mapRootElementToFormLike.set(formLike.rootElement, formLike);
          continue;
        }

        // If the formLike is already present, ensure that the properties are the same.
        info(
          "Checking if the new FormLike for the same root has the same properties"
        );
        formLikeEqual(formLike, existingFormLike);
      }

      if (testcase.beforeGetFunction) {
        await testcase.beforeGetFunction(document);
      }

      Assert.strictEqual(
        mapRootElementToFormLike.size,
        testcase.returnedFieldIDsByFormLike.length,
        "Check the correct number of different formLikes were returned"
      );

      let formLikeIndex = -1;
      for (let formLikeFromInput of mapRootElementToFormLike.values()) {
        formLikeIndex++;
        let pwFields = LoginFormState._getPasswordFields(formLikeFromInput, {
          fieldOverrideRecipe: testcase.fieldOverrideRecipe,
          minPasswordLength: testcase.minPasswordLength,
        });

        if (
          ChromeUtils.getClassName(formLikeFromInput.rootElement) ===
          "HTMLFormElement"
        ) {
          let formLikeFromForm = LoginFormFactory.createFromForm(
            formLikeFromInput.rootElement
          );
          info(
            "Checking that the FormLike created for the <form> matches" +
              " the one from a password field"
          );
          formLikeEqual(formLikeFromInput, formLikeFromForm);
        }

        if (testcase.returnedFieldIDsByFormLike[formLikeIndex].length === 0) {
          Assert.strictEqual(
            pwFields,
            null,
            "If no password fields were found null should be returned"
          );
        } else {
          Assert.strictEqual(
            pwFields.length,
            testcase.returnedFieldIDsByFormLike[formLikeIndex].length,
            "Check the # of password fields for formLike #" + formLikeIndex
          );
        }

        for (
          let i = 0;
          i < testcase.returnedFieldIDsByFormLike[formLikeIndex].length;
          i++
        ) {
          let expectedID =
            testcase.returnedFieldIDsByFormLike[formLikeIndex][i];
          Assert.strictEqual(
            pwFields[i].element.id,
            expectedID,
            "Check password field " + i + " ID"
          );
        }
      }
    });
  })();
}

const EMOJI_TESTCASES = [
  {
    description:
      "Single characters composed of 2 code units should ideally fail minPasswordLength of 2",
    document: `<form>
               <input id="pw" type=password value="💩">
               </form>`,
    returnedFieldIDsByFormLike: [["pw"]],
    minPasswordLength: 2,
  },
  {
    description:
      "Single characters composed of multiple code units should ideally fail minPasswordLength of 2",
    document: `<form>
               <input id="pw" type=password value="👪">
               </form>`,
    minPasswordLength: 2,
  },
];

// Note: Bug 780449 tracks our handling of emoji and multi-code-point characters in password fields
// and the .length we should expect when a password value includes them
for (let tc of EMOJI_TESTCASES) {
  info("Sanity checking the testcase: " + tc.description);

  (function() {
    let testcase = tc;
    add_task(async function() {
      info("Starting testcase: " + testcase.description);
      let document = MockDocument.createTestDocument(
        "http://localhost:8080/test/",
        testcase.document
      );
      let input = document.querySelector("input[type='password']");
      Assert.ok(input, "Found the password field");
      let formLike = LoginFormFactory.createFromField(input);
      let pwFields = LoginFormState._getPasswordFields(formLike, {
        minPasswordLength: testcase.minPasswordLength,
      });
      info("Got password fields: " + pwFields.length);
      todo_check_eq(
        pwFields.length,
        0,
        "Check a single-character (emoji) password is excluded from the password fields collection"
      );
    });
  })();
}
