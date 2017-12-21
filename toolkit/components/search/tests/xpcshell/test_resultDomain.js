/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getResultDomain API.
 */

"use strict";

function run_test() {
  useHttpServer();

  run_next_test();
}

add_task(async function test_resultDomain() {
  let [engine1, engine2, engine3] = await addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "A second test engine", xmlFileName: "engine2.xml" },
    { name: "bacon", details: ["", "bacon", "Search Bacon", "GET",
                               "http://www.bacon.moz/?search={searchTerms}"] },
  ]);

  Assert.equal(engine1.getResultDomain(), "google.com");
  Assert.equal(engine1.getResultDomain("text/html"), "google.com");
  Assert.equal(engine1.getResultDomain("application/x-moz-default-purpose"),
               "purpose.google.com");
  Assert.equal(engine1.getResultDomain("fake-response-type"), "");
  Assert.equal(engine2.getResultDomain(), "duckduckgo.com");
  Assert.equal(engine3.getResultDomain(), "bacon.moz");
});
