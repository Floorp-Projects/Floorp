/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals registerCleanupFunction, add_task */

"use strict";

const ENABLE_PREF = "narrate.enabled";

registerCleanupFunction(() => {
  clearUserPref(ENABLE_PREF);
  teardown();
});

add_task(function* testNarratePref() {
  setup();

  yield spawnInNewReaderTab(TEST_ARTICLE, function() {
    is(content.document.querySelectorAll(NarrateTestUtils.TOGGLE).length, 1,
      "narrate is inserted by default");
  });

  setBoolPref(ENABLE_PREF, false);

  yield spawnInNewReaderTab(TEST_ARTICLE, function() {
    ok(!content.document.querySelector(NarrateTestUtils.TOGGLE),
      "narrate is disabled and is not in reader mode");
  });

  setBoolPref(ENABLE_PREF, true);

  yield spawnInNewReaderTab(TEST_ARTICLE, function() {
    is(content.document.querySelectorAll(NarrateTestUtils.TOGGLE).length, 1,
      "narrate is re-enabled and appears only once");
  });
});
