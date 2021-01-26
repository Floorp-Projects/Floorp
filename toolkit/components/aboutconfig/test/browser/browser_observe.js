/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.modify.boolean", true],
      ["test.aboutconfig.modify.number", 1337],
      [
        "test.aboutconfig.modify.string",
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

add_task(async function test_observe_add_user_pref_before_search() {
  Assert.equal(
    Services.prefs.getPrefType(PREF_NEW),
    Ci.nsIPrefBranch.PREF_INVALID
  );

  await AboutConfigTest.withNewTab(
    async function() {
      this.bypassWarningButton.click();

      // No results are shown after the warning page is dismissed or bypassed,
      // and newly added preferences should not be displayed.
      Preferences.set(PREF_NEW, true);
      Assert.ok(!this.prefsTable.firstElementChild);
      Preferences.reset(PREF_NEW);
    },
    { dontBypassWarning: true }
  );
});

add_task(async function test_observe_add_user_pref() {
  Assert.equal(
    Services.prefs.getPrefType(PREF_NEW),
    Ci.nsIPrefBranch.PREF_INVALID
  );

  await AboutConfigTest.withNewTab(async function() {
    for (let value of [false, true, "", "value", 0, -10]) {
      // A row should be added when a new preference is added.
      Assert.ok(!this.getRow(PREF_NEW));
      Preferences.set(PREF_NEW, value);
      let row = this.getRow(PREF_NEW);
      Assert.equal(row.value, "" + value);

      // The row should stay when the preference is removed.
      Preferences.reset(PREF_NEW);
      Assert.ok(row.hasClass("deleted"));

      // Re-adding the preference from the interface should restore its value.
      row.editColumnButton.click();
      if (value.constructor.name != "Boolean") {
        row.editColumnButton.click();
      }
      Assert.equal(row.value, "" + value);
      Assert.ok(Preferences.get(PREF_NEW) === value);

      // Filtering again after deleting should remove the row.
      Preferences.reset(PREF_NEW);
      this.showAll();
      Assert.ok(!this.getRow(PREF_NEW));

      // Searching for the preference name should give the ability to add it.
      Preferences.reset(PREF_NEW);
      this.search(PREF_NEW);
      row = this.getRow(PREF_NEW);
      Assert.ok(row.hasClass("deleted"));

      // The row for adding should be reused if the new preference is added.
      Preferences.set(PREF_NEW, value);
      Assert.equal(row.value, "" + value);

      // If a new preference does not match the filter it is not displayed.
      Preferences.reset(PREF_NEW);
      this.search(PREF_NEW + ".extra");
      Assert.ok(!this.getRow(PREF_NEW));
      Preferences.set(PREF_NEW, value);
      Assert.ok(!this.getRow(PREF_NEW));

      // Resetting the filter should display the new preference.
      this.showAll();
      Assert.equal(this.getRow(PREF_NEW).value, "" + value);

      // Reset the preference, then continue by adding a different value.
      Preferences.reset(PREF_NEW);
      this.showAll();
    }
  });
});

add_task(async function test_observe_delete_user_pref() {
  for (let value of [true, "value", -10]) {
    Preferences.set(PREF_NEW, value);
    await AboutConfigTest.withNewTab(async function() {
      // Deleting the preference should keep the row.
      let row = this.getRow(PREF_NEW);
      Preferences.reset(PREF_NEW);
      Assert.ok(row.hasClass("deleted"));

      // Filtering again should remove the row.
      this.showAll();
      Assert.ok(!this.getRow(PREF_NEW));
    });
  }
});

add_task(async function test_observe_reset_user_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_BOOLEAN_DEFAULT_TRUE, false]],
  });

  await AboutConfigTest.withNewTab(async function() {
    let row = this.getRow(PREF_BOOLEAN_DEFAULT_TRUE);
    Preferences.reset(PREF_BOOLEAN_DEFAULT_TRUE);
    Assert.ok(!row.hasClass("has-user-value"));
    Assert.equal(row.value, "true");
  });
});

add_task(async function test_observe_modify() {
  await AboutConfigTest.withNewTab(async function() {
    for (let [name, value] of [
      ["test.aboutconfig.modify.boolean", false],
      ["test.aboutconfig.modify.number", -10],
      ["test.aboutconfig.modify.string", "value"],
      [PREF_BOOLEAN_DEFAULT_TRUE, false],
      [PREF_NUMBER_DEFAULT_ZERO, 1],
      [PREF_STRING_DEFAULT_EMPTY, "string"],
    ]) {
      let row = this.getRow(name);
      Assert.notEqual(row.value, "" + value);
      Preferences.set(name, value);
      Assert.equal(row.value, "" + value);

      if (value.constructor.name == "Boolean") {
        continue;
      }

      // Changing the value or removing while editing should not take effect.
      row.editColumnButton.click();
      row.valueInput.value = "42";
      Preferences.reset(name);
      Assert.equal(row.element, this.getRow(name).element);
      Assert.equal(row.valueInput.value, "42");

      // Saving should store the value even if the preference was modified.
      row.editColumnButton.click();
      Assert.equal(row.value, "42");
      Assert.equal(Preferences.get(name), "42");
    }
  });
});
