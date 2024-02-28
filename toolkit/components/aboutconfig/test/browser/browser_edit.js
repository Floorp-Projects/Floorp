/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_MODIFY_PREFIX = "test.aboutconfig.modify";
const PREF_MODIFY_BOOLEAN = "test.aboutconfig.modify.boolean";
const PREF_MODIFY_NUMBER = "test.aboutconfig.modify.number";
const PREF_MODIFY_STRING = "test.aboutconfig.modify.string";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_MODIFY_BOOLEAN, true],
      [PREF_MODIFY_NUMBER, 1337],
      [
        PREF_MODIFY_STRING,
        "the answer to the life the universe and everything",
      ],
    ],
  });

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_BOOLEAN_DEFAULT_TRUE);
    Services.prefs.clearUserPref(PREF_NUMBER_DEFAULT_ZERO);
    Services.prefs.clearUserPref(PREF_STRING_DEFAULT_EMPTY);
  });
});

add_task(async function test_add_user_pref() {
  Assert.equal(
    Services.prefs.getPrefType(PREF_NEW),
    Ci.nsIPrefBranch.PREF_INVALID
  );

  await AboutConfigTest.withNewTab(async function () {
    // The row for a new preference appears when searching for its name.
    Assert.ok(!this.getRow(PREF_NEW));

    for (let [radioIndex, expectedValue, expectedEditingMode] of [
      [0, true, false],
      [1, 0, true],
      [2, "", true],
    ]) {
      this.search(PREF_NEW);
      let row = this.getRow(PREF_NEW);
      Assert.ok(row.hasClass("deleted"));
      Assert.ok(row.hasClass("add"));

      // Adding the preference should set the default for the data type.
      row.element.querySelectorAll("input")[radioIndex].click();
      row.editColumnButton.click();
      Assert.ok(!row.hasClass("deleted"));
      Assert.ok(!row.hasClass("add"));
      Assert.ok(Preferences.get(PREF_NEW) === expectedValue);

      // Number and String preferences should be in edit mode.
      Assert.equal(!!row.valueInput, expectedEditingMode);

      // Repeat the search to verify that the preference remains.
      this.search(PREF_NEW);
      row = this.getRow(PREF_NEW);
      Assert.ok(!row.hasClass("deleted"));
      Assert.ok(!row.hasClass("add"));
      Assert.ok(!row.valueInput);

      // Reset the preference, then continue by adding a different type.
      row.resetColumnButton.click();
      Assert.equal(
        Services.prefs.getPrefType(PREF_NEW),
        Ci.nsIPrefBranch.PREF_INVALID
      );
    }
  });
});

add_task(async function test_delete_user_pref() {
  for (let [radioIndex, testValue] of [
    [0, false],
    [1, -1],
    [2, "value"],
  ]) {
    Preferences.set(PREF_NEW, testValue);
    await AboutConfigTest.withNewTab(async function () {
      // Deleting the preference should keep the row.
      let row = this.getRow(PREF_NEW);
      row.resetColumnButton.click();
      Assert.ok(row.hasClass("deleted"));
      Assert.equal(
        Services.prefs.getPrefType(PREF_NEW),
        Ci.nsIPrefBranch.PREF_INVALID
      );

      // Re-adding the preference should keep the same value.
      Assert.ok(row.element.querySelectorAll("input")[radioIndex].checked);
      row.editColumnButton.click();
      Assert.ok(!row.hasClass("deleted"));
      Assert.ok(Preferences.get(PREF_NEW) === testValue);

      // Filtering again after deleting should remove the row.
      row.resetColumnButton.click();
      this.showAll();
      Assert.ok(!this.getRow(PREF_NEW));
    });
  }
});

add_task(async function test_click_type_label_multiple_forms() {
  // This test displays the row to add a preference while other preferences are
  // also displayed, and tries to select the type of the new preference by
  // clicking the label next to the radio button. This should work even if the
  // user has deleted a different preference, and multiple forms are displayed.
  const PREF_TO_DELETE = "test.aboutconfig.modify.boolean";
  const PREF_NEW_WHILE_DELETED = "test.aboutconfig.modify.";

  await AboutConfigTest.withNewTab(async function () {
    this.search(PREF_NEW_WHILE_DELETED);

    // This preference will remain deleted during the test.
    let existingRow = this.getRow(PREF_TO_DELETE);
    existingRow.resetColumnButton.click();

    let newRow = this.getRow(PREF_NEW_WHILE_DELETED);

    for (let [radioIndex, expectedValue] of [
      [0, true],
      [1, 0],
      [2, ""],
    ]) {
      let radioLabels = newRow.element.querySelectorAll("label > span");
      await this.document.l10n.translateElements(radioLabels);

      // Even if this is the second form on the page, the click should select
      // the radio button next to the label, not the one on the first form.
      EventUtils.synthesizeMouseAtCenter(
        radioLabels[radioIndex],
        {},
        this.browser.contentWindow
      );

      // Adding the preference should set the default for the data type.
      newRow.editColumnButton.click();
      Assert.ok(Preferences.get(PREF_NEW_WHILE_DELETED) === expectedValue);

      // Reset the preference, then continue by adding a different type.
      newRow.resetColumnButton.click();
    }

    // Re-adding the deleted preference should restore the value.
    existingRow.editColumnButton.click();
    Assert.ok(Preferences.get(PREF_TO_DELETE) === true);
  });
});

add_task(async function test_reset_user_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_BOOLEAN_DEFAULT_TRUE, false],
      [PREF_STRING_LOCALIZED_MISSING, "user-value"],
    ],
  });

  await AboutConfigTest.withNewTab(async function () {
    // Click reset.
    let row = this.getRow(PREF_BOOLEAN_DEFAULT_TRUE);
    row.resetColumnButton.click();
    // Check new layout and reset.
    Assert.ok(!row.hasClass("has-user-value"));
    Assert.ok(!row.resetColumnButton);
    Assert.ok(!Services.prefs.prefHasUserValue(PREF_BOOLEAN_DEFAULT_TRUE));
    Assert.equal(this.getRow(PREF_BOOLEAN_DEFAULT_TRUE).value, "true");

    // Filter again to test the preference cache.
    this.showAll();
    row = this.getRow(PREF_BOOLEAN_DEFAULT_TRUE);
    Assert.ok(!row.hasClass("has-user-value"));
    Assert.ok(!row.resetColumnButton);
    Assert.equal(this.getRow(PREF_BOOLEAN_DEFAULT_TRUE).value, "true");

    // Clicking reset on a localized preference without a corresponding value.
    row = this.getRow(PREF_STRING_LOCALIZED_MISSING);
    Assert.equal(row.value, "user-value");
    row.resetColumnButton.click();
    // Check new layout and reset.
    Assert.ok(!row.hasClass("has-user-value"));
    Assert.ok(!row.resetColumnButton);
    Assert.ok(!Services.prefs.prefHasUserValue(PREF_STRING_LOCALIZED_MISSING));
    Assert.equal(this.getRow(PREF_STRING_LOCALIZED_MISSING).value, "");
  });
});

add_task(async function test_modify() {
  await AboutConfigTest.withNewTab(async function () {
    // Test toggle for boolean prefs.
    for (let nameOfBoolPref of [
      PREF_MODIFY_BOOLEAN,
      PREF_BOOLEAN_DEFAULT_TRUE,
    ]) {
      let row = this.getRow(nameOfBoolPref);
      // Do this a two times to reset the pref.
      for (let i = 0; i < 2; i++) {
        row.editColumnButton.click();
        // Check new layout and saving in backend.
        Assert.equal(
          this.getRow(nameOfBoolPref).value,
          "" + Preferences.get(nameOfBoolPref)
        );
        let prefHasUserValue = Services.prefs.prefHasUserValue(nameOfBoolPref);
        Assert.equal(row.hasClass("has-user-value"), prefHasUserValue);
        Assert.equal(!!row.resetColumnButton, prefHasUserValue);
      }
    }

    // Test abort of edit by starting with string and continuing with editing Int pref.
    let row = this.getRow(PREF_MODIFY_STRING);
    row.editColumnButton.click();
    row.valueInput.value = "test";
    let intRow = this.getRow(PREF_MODIFY_NUMBER);
    intRow.editColumnButton.click();
    Assert.equal(intRow.valueInput.value, Preferences.get(PREF_MODIFY_NUMBER));
    Assert.ok(!row.valueInput);
    Assert.equal(row.value, Preferences.get(PREF_MODIFY_STRING));

    // Test validation of integer values.
    for (let invalidValue of [
      "",
      " ",
      "a",
      "1.5",
      "-2147483649",
      "2147483648",
    ]) {
      intRow.valueInput.value = invalidValue;
      intRow.editColumnButton.click();
      // We should still be in edit mode.
      Assert.ok(intRow.valueInput);
    }

    // Test correct saving and DOM-update.
    for (let [prefName, willDelete] of [
      [PREF_MODIFY_STRING, true],
      [PREF_MODIFY_NUMBER, true],
      [PREF_NUMBER_DEFAULT_ZERO, false],
      [PREF_STRING_DEFAULT_EMPTY, false],
    ]) {
      row = this.getRow(prefName);
      // Activate edit and check displaying.
      row.editColumnButton.click();
      Assert.equal(row.valueInput.value, Preferences.get(prefName));
      row.valueInput.value = "42";
      // Save and check saving.
      row.editColumnButton.click();
      Assert.equal(Preferences.get(prefName), "42");
      Assert.equal(row.value, "42");
      Assert.ok(row.hasClass("has-user-value"));
      // Reset or delete the preference while editing.
      row.editColumnButton.click();
      Assert.equal(row.valueInput.value, Preferences.get(prefName));
      row.resetColumnButton.click();
      Assert.ok(!row.hasClass("has-user-value"));
      Assert.equal(row.hasClass("deleted"), willDelete);
    }
  });

  // This test would have opened the invalid form popup, so just close it so as not to
  // affect later tests.
  let invalidFormPopup = window.document.getElementById("invalid-form-popup");
  invalidFormPopup.hidePopup();
  await BrowserTestUtils.waitForCondition(() => {
    return invalidFormPopup.state == "closed";
  }, "form validation popup closed");
});

add_task(async function test_edit_field_selected() {
  let prefsToCheck = [
    [PREF_MODIFY_STRING, "A string", "A new string"],
    [PREF_MODIFY_NUMBER, "100", "500"],
  ];
  await AboutConfigTest.withNewTab(async function () {
    for (let [prefName, startValue, endValue] of prefsToCheck) {
      Preferences.set(prefName, startValue);
      let row = this.getRow(prefName);

      Assert.equal(row.value, startValue);
      row.editColumnButton.click();
      Assert.equal(row.valueInput.value, startValue);
      Assert.equal(
        row.valueInput.getAttribute("aria-label"),
        prefName,
        "The input field is labeled from the pref name"
      );

      EventUtils.sendString(endValue, this.window);

      row.editColumnButton.click();
      Assert.equal(row.value, endValue);
      Assert.equal(Preferences.get(prefName), endValue);
    }
  });
});

add_task(async function test_escape_cancels_edit() {
  await AboutConfigTest.withNewTab(async function () {
    let row = this.getRow(PREF_MODIFY_STRING);
    Preferences.set(PREF_MODIFY_STRING, "Edit me, maybe");

    for (let blurInput of [false, true]) {
      Assert.ok(!row.valueInput);
      row.editColumnButton.click();

      Assert.ok(row.valueInput);

      Assert.equal(row.valueInput.value, "Edit me, maybe");
      row.valueInput.value = "Edited";

      // Test both cases of the input being focused and not being focused.
      if (blurInput) {
        row.valueInput.blur();
        Assert.notEqual(this.document.activeElement, row.valueInput);
      } else {
        Assert.equal(this.document.activeElement, row.valueInput);
      }

      EventUtils.synthesizeKey("KEY_Escape", {}, this.window);

      Assert.ok(!row.valueInput);
      Assert.equal(row.value, "Edit me, maybe");
      Assert.equal(row.value, Preferences.get(PREF_MODIFY_STRING));
    }
  });
});

add_task(async function test_double_click_modify() {
  Preferences.set(PREF_MODIFY_BOOLEAN, true);
  Preferences.set(PREF_MODIFY_NUMBER, 10);
  Preferences.set(PREF_MODIFY_STRING, "Hello!");

  await AboutConfigTest.withNewTab(async function () {
    this.search(PREF_MODIFY_PREFIX);

    let click = (target, opts) =>
      EventUtils.synthesizeMouseAtCenter(target, opts, this.window);
    let doubleClick = target => {
      // We intentionally turn off this a11y check, because the following series
      // of clicks (in these test cases) is either performing an activation of
      // the edit mode for prefs or selecting a text in focused inputs. The
      // edit mode can be activated with a separate "Edit" or "Toggle" button
      // provided for each pref, and the text selection can be performed with
      // caret browsing (when supported). Thus, this rule check can be ignored
      // by a11y_checks suite.
      AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
      // Trigger two mouse events to simulate the first then second click.
      click(target, { clickCount: 1 });
      click(target, { clickCount: 2 });
      AccessibilityUtils.resetEnv();
    };
    let tripleClick = target => {
      // We intentionally turn off this a11y check, because the following series
      // of clicks is purposefully targeting a non - interactive text content.
      // This action does not require the element to have an interactive
      // accessible to be done by assistive technology with caret browsing
      // (when supported), this rule check shall be ignored by a11y_checks suite.
      AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
      // Trigger all 3 mouse events to simulate the three mouse events we'd see.
      click(target, { clickCount: 1 });
      click(target, { clickCount: 2 });
      click(target, { clickCount: 3 });
      AccessibilityUtils.resetEnv();
    };

    // Check double-click to edit a boolean.
    let boolRow = this.getRow(PREF_MODIFY_BOOLEAN);
    Assert.equal(boolRow.value, "true");
    doubleClick(boolRow.valueCell);
    Assert.equal(boolRow.value, "false");
    doubleClick(boolRow.nameCell);
    Assert.equal(boolRow.value, "true");

    // Check double-click to edit a number.
    let intRow = this.getRow(PREF_MODIFY_NUMBER);
    Assert.equal(intRow.value, 10);
    doubleClick(intRow.valueCell);
    Assert.equal(this.document.activeElement, intRow.valueInput);
    EventUtils.sendString("75");
    EventUtils.synthesizeKey("KEY_Enter");
    Assert.equal(intRow.value, 75);

    // Check double-click focuses input when already editing.
    Assert.equal(intRow.value, 75);
    doubleClick(intRow.nameCell);
    Assert.equal(this.document.activeElement, intRow.valueInput);
    intRow.valueInput.blur();
    Assert.notEqual(this.document.activeElement, intRow.valueInput);
    doubleClick(intRow.nameCell);
    Assert.equal(this.document.activeElement, intRow.valueInput);
    EventUtils.sendString("20");
    EventUtils.synthesizeKey("KEY_Enter");
    Assert.equal(intRow.value, 20);

    // Check double-click to edit a string.
    let stringRow = this.getRow(PREF_MODIFY_STRING);
    Assert.equal(stringRow.value, "Hello!");
    doubleClick(stringRow.valueCell);
    Assert.equal(
      this.document.activeElement,
      stringRow.valueInput,
      "The input is focused"
    );
    EventUtils.sendString("New String!");
    EventUtils.synthesizeKey("KEY_Enter");
    Assert.equal(stringRow.value, "New String!");

    // Check triple-click also edits the pref and selects the text inside.
    tripleClick(stringRow.nameCell);
    Assert.equal(
      this.document.activeElement,
      stringRow.valueInput,
      "The input is focused"
    );

    // Check double-click inside input selects a word.
    let newString = "Another string...";
    EventUtils.sendString(newString);
    Assert.equal(this.window.getSelection().toString(), "");
    let stringInput = stringRow.valueInput;
    doubleClick(stringInput);
    let selectionLength = stringInput.selectionEnd - stringInput.selectionStart;
    Assert.greater(selectionLength, 0);
    Assert.less(selectionLength, newString.length);
    EventUtils.synthesizeKey("KEY_Enter");
    Assert.equal(stringRow.value, newString);

    // Check that double/triple-click on the add row selects text as usual.
    let addRow = this.getRow(PREF_MODIFY_PREFIX);
    Assert.ok(addRow.hasClass("deleted"));
    doubleClick(addRow.nameCell);
    Assert.ok(PREF_MODIFY_PREFIX.includes(this.window.getSelection()));
    tripleClick(addRow.nameCell);
    Assert.equal(this.window.getSelection().toString(), PREF_MODIFY_PREFIX);
    // Make sure the localized text is set in the value cell.
    let labels = Array.from(addRow.valueCell.querySelectorAll("label > span"));
    await this.document.l10n.translateElements(labels);
    Assert.ok(labels.every(label => !!label.textContent));
    // Double-click the first input label text.
    doubleClick(labels[0]);
    Assert.equal(this.window.getSelection().toString(), labels[0].textContent);
    tripleClick(addRow.valueCell.querySelector("label > span"));
    Assert.equal(
      this.window.getSelection().toString(),
      labels.map(l => l.textContent).join("")
    );
  });
});
