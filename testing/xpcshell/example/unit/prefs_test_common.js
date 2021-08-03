/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function isValidPref(prefName) {
  return Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_INVALID;
}

// Check a pref that appears in testing/profiles/xpcshell/user.js
// but NOT in StaticPrefList.yaml, modules/libpref/init/all.js
function has_pref_from_xpcshell_user_js() {
  return isValidPref("extensions.webextensions.warnings-as-errors");
}

// Test pref from xpcshell-with-prefs.ini
function has_pref_from_manifest_defaults() {
  return isValidPref("dummy.pref.from.test.manifest");
}

// Test pref set in xpcshell.ini and xpcshell-with-prefs.ini
function has_pref_from_manifest_file_section() {
  return isValidPref("dummy.pref.from.test.file");
}

function check_common_xpcshell_with_prefs() {
  Assert.ok(
    has_pref_from_xpcshell_user_js(),
    "Should have pref from xpcshell's user.js"
  );

  Assert.ok(
    has_pref_from_manifest_defaults(),
    "Should have pref from DEFAULTS in xpcshell-with-prefs.ini"
  );
}

function check_common_xpcshell_without_prefs() {
  Assert.ok(
    has_pref_from_xpcshell_user_js(),
    "Should have pref from xpcshell's user.js"
  );

  Assert.ok(
    !has_pref_from_manifest_defaults(),
    "xpcshell.ini did not set any prefs in DEFAULTS"
  );
}
