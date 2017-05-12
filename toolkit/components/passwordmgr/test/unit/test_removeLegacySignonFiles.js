/**
 * Tests the LoginHelper object.
 */

"use strict";



XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");


async function createSignonFile(singon) {
  let {file, pref}  = singon;

  if (pref) {
    Services.prefs.setCharPref(pref, file);
  }

  await OS.File.writeAtomic(
    OS.Path.join(OS.Constants.Path.profileDir, file), new Uint8Array(1));
}

async function isSignonClear(singon) {
  const {file, pref} = singon;
  const fileExists = await OS.File.exists(
    OS.Path.join(OS.Constants.Path.profileDir, file));

  if (pref) {
    try {
      Services.prefs.getCharPref(pref);
      return false;
    } catch (e) {}
  }

  return !fileExists;
}

add_task(async function test_remove_lagecy_signonfile() {
  // In the last test case, signons3.txt being deleted even when
  // it doesn't exist.
  const signonsSettings = [[
    { file: "signons.txt" },
    { file: "signons2.txt" },
    { file: "signons3.txt" }
  ], [
    { file: "signons.txt", pref: "signon.SignonFileName" },
    { file: "signons2.txt", pref: "signon.SignonFileName2" },
    { file: "signons3.txt", pref: "signon.SignonFileName3" }
  ], [
    { file: "signons2.txt" },
    { file: "singons.txt", pref: "signon.SignonFileName" },
    { file: "customized2.txt", pref: "signon.SignonFileName2" },
    { file: "customized3.txt", pref: "signon.SignonFileName3" }
  ]];

  for (let setting of signonsSettings) {
    for (let singon of setting) {
      await createSignonFile(singon);
    }

    LoginHelper.removeLegacySignonFiles();

    for (let singon of setting) {
      equal(await isSignonClear(singon), true);
    }
  }
});
