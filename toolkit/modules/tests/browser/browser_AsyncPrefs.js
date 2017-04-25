"use strict";

const kWhiteListedBool = "testing.allowed-prefs.some-bool-pref";
const kWhiteListedChar = "testing.allowed-prefs.some-char-pref";
const kWhiteListedInt = "testing.allowed-prefs.some-int-pref";

function resetPrefs() {
  for (let pref of [kWhiteListedBool, kWhiteListedChar, kWhiteListedBool]) {
    Services.prefs.clearUserPref(pref);
  }
}

registerCleanupFunction(resetPrefs);

Services.prefs.getDefaultBranch("testing.allowed-prefs.").setBoolPref("some-bool-pref", false);
Services.prefs.getDefaultBranch("testing.allowed-prefs.").setCharPref("some-char-pref", "");
Services.prefs.getDefaultBranch("testing.allowed-prefs.").setIntPref("some-int-pref", 0);

function* runTest() {
  let {AsyncPrefs} = Cu.import("resource://gre/modules/AsyncPrefs.jsm", {});
  const kInChildProcess = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

  // Need to define these again because when run in a content task we have no scope access.
  const kNotWhiteListed = "some.pref.thats.not.whitelisted";
  const kWhiteListedBool = "testing.allowed-prefs.some-bool-pref";
  const kWhiteListedChar = "testing.allowed-prefs.some-char-pref";
  const kWhiteListedInt = "testing.allowed-prefs.some-int-pref";

  const procDesc = kInChildProcess ? "child process" : "parent process";

  const valueResultMap = [
    [true, "Bool"],
    [false, "Bool"],
    [10, "Int"],
    [-1, "Int"],
    ["", "Char"],
    ["stuff", "Char"],
    [[], false],
    [{}, false],
    [Services.io.newURI("http://mozilla.org/"), false],
  ];

  const prefMap = [
    ["Bool", kWhiteListedBool],
    ["Char", kWhiteListedChar],
    ["Int", kWhiteListedInt],
  ];

  function doesFail(pref, value) {
    let msg = `Should not succeed setting ${pref} to ${value} in ${procDesc}`;
    return AsyncPrefs.set(pref, value).then(() => ok(false, msg), error => ok(true, msg + "; " + error));
  }

  function doesWork(pref, value) {
    let msg = `Should be able to set ${pref} to ${value} in ${procDesc}`;
    return AsyncPrefs.set(pref, value).then(() => ok(true, msg), error => ok(false, msg + "; " + error));
  }

  function doReset(pref) {
    let msg = `Should be able to reset ${pref} in ${procDesc}`;
    return AsyncPrefs.reset(pref).then(() => ok(true, msg), () => ok(false, msg));
  }

  for (let [val, ] of valueResultMap) {
    yield doesFail(kNotWhiteListed, val);
    is(Services.prefs.prefHasUserValue(kNotWhiteListed), false, "Pref shouldn't get changed");
  }

  let resetMsg = `Should not succeed resetting ${kNotWhiteListed} in ${procDesc}`;
  AsyncPrefs.reset(kNotWhiteListed).then(() => ok(false, resetMsg), error => ok(true, resetMsg + "; " + error));

  for (let [type, pref] of prefMap) {
    for (let [val, result] of valueResultMap) {
      if (result == type) {
        yield doesWork(pref, val);
        is(Services.prefs["get" + type + "Pref"](pref), val, "Pref should have been updated");
        yield doReset(pref);
      } else {
        yield doesFail(pref, val);
        is(Services.prefs.prefHasUserValue(pref), false, `Pref ${pref} shouldn't get changed`);
      }
    }
  }
}

add_task(function* runInParent() {
  yield runTest();
  resetPrefs();
});

if (gMultiProcessBrowser) {
  add_task(function* runInChild() {
    ok(gBrowser.selectedBrowser.isRemoteBrowser, "Should actually run this in child process");
    yield ContentTask.spawn(gBrowser.selectedBrowser, null, runTest);
    resetPrefs();
  });
}
