/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getResultDomain API.
 */

"use strict";

add_task(async function setup() {
  await SearchTestUtils.useTestEngines("data", null);
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_resultDomain() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("Test search engine");

  Assert.equal(engine.getResultDomain(), "www.google.com");
  Assert.equal(engine.getResultDomain("text/html"), "www.google.com");
  Assert.equal(
    engine.getResultDomain("application/x-suggestions+json"),
    "suggestqueries.google.com"
  );
  Assert.equal(engine.getResultDomain("fake-response-type"), "");
});
