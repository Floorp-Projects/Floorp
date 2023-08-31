/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/modules/test/browser/head.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storage_access_head.js",
  this
);

// Callback function to check if the user activation remained active
async function requestStorageAccessAndExpectUserActivationActive() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  let p = document.requestStorageAccess();
  try {
    await p;
    ok(true, "gain storage access.");
  } catch {
    ok(false, "denied storage access.");
  }

  is(
    SpecialPowers.wrap(document).hasBeenUserGestureActivated,
    true,
    `check has-been-user-activated on the top-level-document`
  );
  is(
    SpecialPowers.wrap(document).hasValidTransientUserGestureActivation,
    true,
    `check has-valid-transient-user-activation on the top-level-document`
  );

  SpecialPowers.wrap(document).clearUserGestureActivation();
}

// Callback function to check if the user activation was consumed
async function requestStorageAccessAndExpectUserActivationConsumed() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  const lastUserGesture = SpecialPowers.wrap(document).lastUserGestureTimeStamp;

  let p = document.requestStorageAccess();
  try {
    await p;
    ok(true, "gain storage access.");
  } catch {
    ok(false, "denied storage access.");
  }

  is(
    SpecialPowers.wrap(document).hasBeenUserGestureActivated,
    true,
    `check has-been-user-activated on the top-level-document`
  );
  is(
    SpecialPowers.wrap(document).lastUserGestureTimeStamp,
    lastUserGesture,
    `check has-valid-transient-user-activation on the top-level-document`
  );

  SpecialPowers.wrap(document).clearUserGestureActivation();
}

add_task(async function testActiveEvent() {
  await setPreferences();

  await openPageAndRunCode(
    TEST_TOP_PAGE_HTTPS,
    getExpectPopupAndClick("accept"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectUserActivationActive
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async function testConsumedEvent() {
  await setPreferences();
  SpecialPowers.Services.prefs.setIntPref(
    "dom.user_activation.transient.timeout",
    1000
  );

  const timeout =
    SpecialPowers.getIntPref("dom.user_activation.transient.timeout") + 1000;
  // Timeout to consume transient activation
  await openPageAndRunCode(
    TEST_TOP_PAGE_HTTPS,
    getExpectPopupAndClickAfterDelay("accept", timeout),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectUserActivationConsumed
  );
  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});
