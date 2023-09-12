/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests searchUrlDomain API.
 */

"use strict";

add_setup(async function () {
  await SearchTestUtils.useTestEngines("data", null);
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_resultDomain() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("Test search engine");

  Assert.equal(engine.searchUrlDomain, "www.google.com");
});
