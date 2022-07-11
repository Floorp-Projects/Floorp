/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests that static preferences in the content process
// correctly handle values which are different from their
// statically-defined defaults.
//
// Since we can't access static preference values from JS, this tests relies on
// assertions in debug builds to detect mismatches. The default and user
// values of two preferences are changed (respectively) before a content
// process is started. Once the content process is launched, the
// preference service asserts that the values stored in all static prefs
// match their current values as known to the preference service. If
// there's a mismatch, the shell will crash, and the test will fail.
//
// For sanity, we also check that the dynamically retrieved preference
// values in the content process match our expectations, though this is
// not strictly part of the test.

const PREF1_NAME = "dom.webcomponents.shadowdom.report_usage";
const PREF1_VALUE = false;

const PREF2_NAME = "dom.mutation-events.cssom.disabled";
const PREF2_VALUE = true;

const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

XPCShellContentUtils.init(this);

const { prefs } = Services;
const defaultPrefs = prefs.getDefaultBranch("");

add_task(async function test_sharedMap_static_prefs() {
  equal(
    prefs.getBoolPref(PREF1_NAME),
    PREF1_VALUE,
    `Expected initial value for ${PREF1_NAME}`
  );
  equal(
    prefs.getBoolPref(PREF2_NAME),
    PREF2_VALUE,
    `Expected initial value for ${PREF2_NAME}`
  );

  defaultPrefs.setBoolPref(PREF1_NAME, !PREF1_VALUE);
  prefs.setBoolPref(PREF2_NAME, !PREF2_VALUE);

  equal(
    prefs.getBoolPref(PREF1_NAME),
    !PREF1_VALUE,
    `Expected updated value for ${PREF1_NAME}`
  );
  equal(
    prefs.getBoolPref(PREF2_NAME),
    !PREF2_VALUE,
    `Expected updated value for ${PREF2_NAME}`
  );

  let contentPage = await XPCShellContentUtils.loadContentPage("about:blank", {
    remote: true,
  });
  registerCleanupFunction(() => contentPage.close());

  /* eslint-disable no-shadow */
  let values = await contentPage.spawn([PREF1_NAME, PREF2_NAME], prefs => {
    return prefs.map(pref => Services.prefs.getBoolPref(pref));
  });
  /* eslint-enable no-shadow */

  equal(values[0], !PREF1_VALUE, `Expected content value for ${PREF1_NAME}`);
  equal(values[1], !PREF2_VALUE, `Expected content value for ${PREF2_NAME}`);
});
