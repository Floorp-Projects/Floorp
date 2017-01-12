/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getAlternateDomains API.
 */

"use strict";

function run_test() {
  removeMetadata();
  useHttpServer();

  run_next_test();
}

add_task(function* test_parseSubmissionURL() {
  // Hide the default engines to prevent them from being used in the search.
  for (let engine of Services.search.getEngines()) {
    Services.search.removeEngine(engine);
  }

  let [engine1, engine2, engine3, engine4] = yield addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "Test search engine (fr)", xmlFileName: "engine-fr.xml" },
    { name: "bacon_addParam", details: ["", "bacon_addParam", "Search Bacon",
                                        "GET", "http://www.bacon.test/find"] },
    { name: "idn_addParam", details: ["", "idn_addParam", "Search IDN",
                                        "GET", "http://www.xn--bcher-kva.ch/search"] },
    // The following engines cannot identify the search parameter.
    { name: "A second test engine", xmlFileName: "engine2.xml" },
    { name: "bacon", details: ["", "bacon", "Search Bacon", "GET",
                               "http://www.bacon.moz/search?q={searchTerms}"] },
  ]);

  engine3.addParam("q", "{searchTerms}", null);
  engine4.addParam("q", "{searchTerms}", null);

  // Test the first engine, whose URLs use UTF-8 encoding.
  let url = "http://www.google.com/search?foo=bar&q=caff%C3%A8";
  let result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "caff\u00E8");
  do_check_true(url.slice(result.termsOffset).startsWith("caff%C3%A8"));
  do_check_eq(result.termsLength, "caff%C3%A8".length);

  // The second engine uses a locale-specific domain that is an alternate domain
  // of the first one, but the second engine should get priority when matching.
  // The URL used with this engine uses ISO-8859-1 encoding instead.
  url = "http://www.google.fr/search?q=caff%E8";
  result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine2);
  do_check_eq(result.terms, "caff\u00E8");
  do_check_true(url.slice(result.termsOffset).startsWith("caff%E8"));
  do_check_eq(result.termsLength, "caff%E8".length);

  // Test a domain that is an alternate domain of those defined.  In this case,
  // the first matching engine from the ordered list should be returned.
  url = "http://www.google.co.uk/search?q=caff%C3%A8";
  result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "caff\u00E8");
  do_check_true(url.slice(result.termsOffset).startsWith("caff%C3%A8"));
  do_check_eq(result.termsLength, "caff%C3%A8".length);

  // We support parsing URLs from a dynamically added engine.  Those engines use
  // windows-1252 encoding by default.
  url = "http://www.bacon.test/find?q=caff%E8";
  result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine3);
  do_check_eq(result.terms, "caff\u00E8");
  do_check_true(url.slice(result.termsOffset).startsWith("caff%E8"));
  do_check_eq(result.termsLength, "caff%E8".length);

  // Test URLs with unescaped unicode characters.
  url = "http://www.google.com/search?q=foo+b\u00E4r";
  result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "foo b\u00E4r");
  do_check_true(url.slice(result.termsOffset).startsWith("foo+b\u00E4r"));
  do_check_eq(result.termsLength, "foo+b\u00E4r".length);

  // Test search engines with unescaped IDNs.
  url = "http://www.b\u00FCcher.ch/search?q=foo+bar";
  result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine4);
  do_check_eq(result.terms, "foo bar");
  do_check_true(url.slice(result.termsOffset).startsWith("foo+bar"));
  do_check_eq(result.termsLength, "foo+bar".length);

  // Test search engines with escaped IDNs.
  url = "http://www.xn--bcher-kva.ch/search?q=foo+bar";
  result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine4);
  do_check_eq(result.terms, "foo bar");
  do_check_true(url.slice(result.termsOffset).startsWith("foo+bar"));
  do_check_eq(result.termsLength, "foo+bar".length);

  // Parsing of parameters from an engine template URL is not supported.
  do_check_eq(Services.search.parseSubmissionURL(
                              "http://www.bacon.moz/search?q=").engine, null);
  do_check_eq(Services.search.parseSubmissionURL(
                              "https://duckduckgo.com?q=test").engine, null);
  do_check_eq(Services.search.parseSubmissionURL(
                              "https://duckduckgo.com/?q=test").engine, null);

  // HTTP and HTTPS schemes are interchangeable.
  url = "https://www.google.com/search?q=caff%C3%A8";
  result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "caff\u00E8");
  do_check_true(url.slice(result.termsOffset).startsWith("caff%C3%A8"));

  // Decoding search terms with multiple spaces should work.
  result = Services.search.parseSubmissionURL(
             "http://www.google.com/search?q=+with++spaces+");
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, " with  spaces ");

  // An empty query parameter should work the same.
  url = "http://www.google.com/search?q=";
  result = Services.search.parseSubmissionURL(url);
  do_check_eq(result.engine, engine1);
  do_check_eq(result.terms, "");
  do_check_eq(result.termsOffset, url.length);

  // There should be no match when the path is different.
  result = Services.search.parseSubmissionURL(
             "http://www.google.com/search/?q=test");
  do_check_eq(result.engine, null);
  do_check_eq(result.terms, "");
  do_check_eq(result.termsOffset, -1);

  // There should be no match when the argument is different.
  result = Services.search.parseSubmissionURL(
             "http://www.google.com/search?q2=test");
  do_check_eq(result.engine, null);
  do_check_eq(result.terms, "");
  do_check_eq(result.termsOffset, -1);

  // There should be no match for URIs that are not HTTP or HTTPS.
  result = Services.search.parseSubmissionURL(
             "file://localhost/search?q=test");
  do_check_eq(result.engine, null);
  do_check_eq(result.terms, "");
  do_check_eq(result.termsOffset, -1);
});
