/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getAlternateDomains API.
 */

"use strict";

function run_test() {
  removeMetadata();
  updateAppInfo();
  useHttpServer();

  run_next_test();
}

add_task(function* test_parseSubmissionURL() {
  // Hide the default engines to prevent them from being used in the search.
  for (let engine of Services.search.getEngines()) {
    Services.search.removeEngine(engine);
  }

  let [engine1, engine2, engine3] = yield addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "Test search engine (fr)", xmlFileName: "engine-fr.xml" },
    { name: "bacon_addParam", details: ["", "bacon_addParam", "Search Bacon",
                                        "GET", "http://www.bacon.test/find"] },
    // The following engines cannot identify the search parameter.
    { name: "A second test engine", xmlFileName: "engine2.xml" },
    { name: "Sherlock test search engine", srcFileName: "engine.src",
      iconFileName: "ico-size-16x16-png.ico" },
    { name: "bacon", details: ["", "bacon", "Search Bacon", "GET",
                               "http://www.bacon.moz/search?q={searchTerms}"] },
  ]);

  engine3.addParam("q", "{searchTerms}", null);

  // Test the first engine, whose URLs use UTF-8 encoding.
  let result = Services.search.parseSubmissionURL(
                               "http://www.google.com/search?q=caff%C3%A8");
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "caff\u00E8");

  // The second engine uses a locale-specific domain that is an alternate domain
  // of the first one, but the second engine should get priority when matching.
  // The URL used with this engine uses ISO-8859-1 encoding instead.
  let result = Services.search.parseSubmissionURL(
                               "http://www.google.fr/search?q=caff%E8");
  do_check_eq(result.engine, engine2);
  do_check_eq(result.terms, "caff\u00E8");

  // Test a domain that is an alternate domain of those defined.  In this case,
  // the first matching engine from the ordered list should be returned.
  let result = Services.search.parseSubmissionURL(
                               "http://www.google.co.uk/search?q=caff%C3%A8");
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "caff\u00E8");

  // We support parsing URLs from a dynamically added engine.  Those engines use
  // windows-1252 encoding by default.
  let result = Services.search.parseSubmissionURL(
                               "http://www.bacon.test/find?q=caff%E8");
  do_check_eq(result.engine, engine3);
  do_check_eq(result.terms, "caff\u00E8");

  // Parsing of parameters from an engine template URL is not supported.
  do_check_eq(Services.search.parseSubmissionURL(
                              "http://www.bacon.moz/search?q=").engine, null);
  do_check_eq(Services.search.parseSubmissionURL(
                              "https://duckduckgo.com?q=test").engine, null);
  do_check_eq(Services.search.parseSubmissionURL(
                              "https://duckduckgo.com/?q=test").engine, null);

  // Sherlock engines are not supported.
  do_check_eq(Services.search.parseSubmissionURL(
                              "http://getfirefox.com?q=test").engine, null);
  do_check_eq(Services.search.parseSubmissionURL(
                              "http://getfirefox.com/?q=test").engine, null);

  // HTTP and HTTPS schemes are interchangeable.
  let result = Services.search.parseSubmissionURL(
                               "https://www.google.com/search?q=caff%C3%A8");
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "caff\u00E8");

  // An empty query parameter should work the same.
  let result = Services.search.parseSubmissionURL(
                               "http://www.google.com/search?q=");
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "");

  // There should be no match when the path is different.
  let result = Services.search.parseSubmissionURL(
                               "http://www.google.com/search/?q=test");
  do_check_eq(result.engine, null);
  do_check_eq(result.terms, "");

  // There should be no match when the argument is different.
  let result = Services.search.parseSubmissionURL(
                               "http://www.google.com/search?q2=test");
  do_check_eq(result.engine, null);
  do_check_eq(result.terms, "");

  // There should be no match for URIs that are not HTTP or HTTPS.
  let result = Services.search.parseSubmissionURL(
                               "file://localhost/search?q=test");
  do_check_eq(result.engine, null);
  do_check_eq(result.terms, "");
});
