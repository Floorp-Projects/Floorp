/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PrefUtils } = ChromeUtils.import(
  "resource://normandy/lib/PrefUtils.jsm"
);

add_task(function getPrefGetsValues() {
  const defaultBranch = Services.prefs.getDefaultBranch("");
  const userBranch = Services.prefs;

  defaultBranch.setBoolPref("test.bool", false);
  userBranch.setBoolPref("test.bool", true);
  defaultBranch.setIntPref("test.int", 1);
  userBranch.setIntPref("test.int", 2);
  defaultBranch.setStringPref("test.string", "default");
  userBranch.setStringPref("test.string", "user");

  equal(
    PrefUtils.getPref("test.bool", { branch: "user" }),
    true,
    "should read user branch bools"
  );
  equal(
    PrefUtils.getPref("test.int", { branch: "user" }),
    2,
    "should read user branch ints"
  );
  equal(
    PrefUtils.getPref("test.string", { branch: "user" }),
    "user",
    "should read user branch strings"
  );

  equal(
    PrefUtils.getPref("test.bool", { branch: "default" }),
    false,
    "should read default branch bools"
  );
  equal(
    PrefUtils.getPref("test.int", { branch: "default" }),
    1,
    "should read default branch ints"
  );
  equal(
    PrefUtils.getPref("test.string", { branch: "default" }),
    "default",
    "should read default branch strings"
  );

  equal(
    PrefUtils.getPref("test.bool"),
    true,
    "should read bools from the user branch by default"
  );
  equal(
    PrefUtils.getPref("test.int"),
    2,
    "should read ints from the user branch by default"
  );
  equal(
    PrefUtils.getPref("test.string"),
    "user",
    "should read strings from the user branch by default"
  );

  equal(
    PrefUtils.getPref("test.does_not_exist"),
    null,
    "Should return null for non-existent prefs by default"
  );
  let defaultValue = Symbol();
  equal(
    PrefUtils.getPref("test.does_not_exist", { defaultValue }),
    defaultValue,
    "Should use the passed default value"
  );
});

// This is an important test because the pref system can behave in strange ways
// when the user branch has a value, but the default branch does not.
add_task(function getPrefHandlesUserValueNoDefaultValue() {
  Services.prefs.setStringPref("test.only-user-value", "user");

  let defaultValue = Symbol();
  equal(
    PrefUtils.getPref("test.only-user-value", {
      branch: "default",
      defaultValue,
    }),
    defaultValue
  );
  equal(PrefUtils.getPref("test.only-user-value", { branch: "default" }), null);
  equal(PrefUtils.getPref("test.only-user-value", { branch: "user" }), "user");
  equal(PrefUtils.getPref("test.only-user-value"), "user");
});

add_task(function getPrefInvalidBranch() {
  Assert.throws(
    () => PrefUtils.getPref("test.pref", { branch: "invalid" }),
    PrefUtils.UnexpectedPreferenceBranch
  );
});

add_task(function setPrefSetsValues() {
  const defaultBranch = Services.prefs.getDefaultBranch("");
  const userBranch = Services.prefs;

  defaultBranch.setIntPref("test.int", 1);
  userBranch.setIntPref("test.int", 2);
  defaultBranch.setStringPref("test.string", "default");
  userBranch.setStringPref("test.string", "user");
  defaultBranch.setBoolPref("test.bool", false);
  userBranch.setBoolPref("test.bool", true);

  PrefUtils.setPref("test.int", 3);
  equal(
    userBranch.getIntPref("test.int"),
    3,
    "the user branch should change for ints"
  );
  PrefUtils.setPref("test.int", 4, { branch: "default" });
  equal(
    userBranch.getIntPref("test.int"),
    3,
    "changing the default branch shouldn't affect the user branch for ints"
  );
  PrefUtils.setPref("test.int", null, { branch: "user" });
  equal(
    userBranch.getIntPref("test.int"),
    4,
    "clearing the user branch should reveal the default value for ints"
  );

  PrefUtils.setPref("test.string", "user override");
  equal(
    userBranch.getStringPref("test.string"),
    "user override",
    "the user branch should change for strings"
  );
  PrefUtils.setPref("test.string", "default override", { branch: "default" });
  equal(
    userBranch.getStringPref("test.string"),
    "user override",
    "changing the default branch shouldn't affect the user branch for strings"
  );
  PrefUtils.setPref("test.string", null, { branch: "user" });
  equal(
    userBranch.getStringPref("test.string"),
    "default override",
    "clearing the user branch should reveal the default value for strings"
  );

  PrefUtils.setPref("test.bool", false);
  equal(
    userBranch.getBoolPref("test.bool"),
    false,
    "the user branch should change for bools"
  );
  // The above effectively unsets the user branch, since it is now the same as the default branch
  PrefUtils.setPref("test.bool", true, { branch: "default" });
  equal(
    userBranch.getBoolPref("test.bool"),
    true,
    "the default branch should change for bools"
  );

  defaultBranch.setBoolPref("test.bool", false);
  userBranch.setBoolPref("test.bool", true);
  equal(
    userBranch.getBoolPref("test.bool"),
    true,
    "the precondition should hold"
  );
  PrefUtils.setPref("test.bool", null, { branch: "user" });
  equal(
    userBranch.getBoolPref("test.bool"),
    false,
    "setting the user branch to null should reveal the default value for bools"
  );
});

add_task(function setPrefInvalidBranch() {
  Assert.throws(
    () => PrefUtils.setPref("test.pref", "value", { branch: "invalid" }),
    PrefUtils.UnexpectedPreferenceBranch
  );
});

add_task(function clearPrefClearsValues() {
  const defaultBranch = Services.prefs.getDefaultBranch("");
  const userBranch = Services.prefs;

  defaultBranch.setStringPref("test.string", "default");
  userBranch.setStringPref("test.string", "user");
  equal(
    userBranch.getStringPref("test.string"),
    "user",
    "the precondition should hold"
  );
  PrefUtils.clearPref("test.string");
  equal(
    userBranch.getStringPref("test.string"),
    "default",
    "clearing the user branch should reveal the default value for bools"
  );

  PrefUtils.clearPref("test.string", { branch: "default" });
  equal(
    userBranch.getStringPref("test.string"),
    "default",
    "clearing the default branch shouldn't do anything"
  );
});

add_task(function clearPrefInvalidBranch() {
  Assert.throws(
    () => PrefUtils.clearPref("test.pref", { branch: "invalid" }),
    PrefUtils.UnexpectedPreferenceBranch
  );
});
