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

add_task(async function testInsecureContext() {
  await setPreferences();

  await openPageAndRunCode(
    TEST_TOP_PAGE_HTTPS,
    getExpectPopupAndClick("accept"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectSuccess
  );

  await openPageAndRunCode(
    TEST_TOP_PAGE,
    expectNoPopup,
    TEST_3RD_PARTY_PAGE_HTTP,
    requestStorageAccessAndExpectFailure
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});
