/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Bug 1781025 - ESLint's use of multi-ini doesn't fully cope with
// mozilla-central's use of .ini files
// eslint-disable-next-line no-unused-vars
function run_test() {
  /* import-globals-from prefs_test_common.js */
  load("prefs_test_common.js");

  check_common_xpcshell_with_prefs();

  Assert.ok(
    has_pref_from_manifest_file_section(),
    "Should have pref set for file in xpcshell-with-prefs.ini"
  );

  Assert.equal(
    Services.prefs.getIntPref("dummy.pref.from.test.file"),
    2,
    "Value of pref that was set once at the file in xpcshell-with-prefs.ini"
  );

  Assert.equal(
    Services.prefs.getStringPref("dummy.pref.from.test.duplicate"),
    "final",
    "The last pref takes precedence when duplicated"
  );

  Assert.equal(
    Services.prefs.getIntPref("dummy.pref.from.test.manifest"),
    1337,
    "File-specific pref takes precedence over manifest defaults"
  );

  Assert.equal(
    Services.prefs.getStringPref("dummy.pref.from.test.ancestor"),
    "Ancestor",
    "Pref in manifest defaults without file-specific override should be set"
  );
}
