/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getAlternateDomains API.
 */

"use strict";

function run_test() {
  useHttpServer();

  run_next_test();
}

add_task(async function test_parseSubmissionURL() {
  // Hide the default engines to prevent them from being used in the search.
  for (let engine of Services.search.getEngines()) {
    Services.search.removeEngine(engine);
  }

  let [engine1, engine2, engine3, engine4] = await addTestEngines([
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
  Assert.equal(result.engine, engine1);
  Assert.equal(result.terms, "caff\u00E8");
  Assert.ok(url.slice(result.termsOffset).startsWith("caff%C3%A8"));
  Assert.equal(result.termsLength, "caff%C3%A8".length);

  // The second engine uses a locale-specific domain that is an alternate domain
  // of the first one, but the second engine should get priority when matching.
  // The URL used with this engine uses ISO-8859-1 encoding instead.
  url = "http://www.google.fr/search?q=caff%E8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine2);
  Assert.equal(result.terms, "caff\u00E8");
  Assert.ok(url.slice(result.termsOffset).startsWith("caff%E8"));
  Assert.equal(result.termsLength, "caff%E8".length);

  // Test a domain that is an alternate domain of those defined.  In this case,
  // the first matching engine from the ordered list should be returned.
  url = "http://www.google.co.uk/search?q=caff%C3%A8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine1);
  Assert.equal(result.terms, "caff\u00E8");
  Assert.ok(url.slice(result.termsOffset).startsWith("caff%C3%A8"));
  Assert.equal(result.termsLength, "caff%C3%A8".length);

  // We support parsing URLs from a dynamically added engine.  Those engines use
  // windows-1252 encoding by default.
  url = "http://www.bacon.test/find?q=caff%E8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine3);
  Assert.equal(result.terms, "caff\u00E8");
  Assert.ok(url.slice(result.termsOffset).startsWith("caff%E8"));
  Assert.equal(result.termsLength, "caff%E8".length);

  // Test URLs with unescaped unicode characters.
  url = "http://www.google.com/search?q=foo+b\u00E4r";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine1);
  Assert.equal(result.terms, "foo b\u00E4r");
  Assert.ok(url.slice(result.termsOffset).startsWith("foo+b\u00E4r"));
  Assert.equal(result.termsLength, "foo+b\u00E4r".length);

  // Test search engines with unescaped IDNs.
  url = "http://www.b\u00FCcher.ch/search?q=foo+bar";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine4);
  Assert.equal(result.terms, "foo bar");
  Assert.ok(url.slice(result.termsOffset).startsWith("foo+bar"));
  Assert.equal(result.termsLength, "foo+bar".length);

  // Test search engines with escaped IDNs.
  url = "http://www.xn--bcher-kva.ch/search?q=foo+bar";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine4);
  Assert.equal(result.terms, "foo bar");
  Assert.ok(url.slice(result.termsOffset).startsWith("foo+bar"));
  Assert.equal(result.termsLength, "foo+bar".length);

  // Parsing of parameters from an engine template URL is not supported.
  Assert.equal(Services.search.parseSubmissionURL(
                               "http://www.bacon.moz/search?q=").engine, null);
  Assert.equal(Services.search.parseSubmissionURL(
                               "https://duckduckgo.com?q=test").engine, null);
  Assert.equal(Services.search.parseSubmissionURL(
                               "https://duckduckgo.com/?q=test").engine, null);

  // HTTP and HTTPS schemes are interchangeable.
  url = "https://www.google.com/search?q=caff%C3%A8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine1);
  Assert.equal(result.terms, "caff\u00E8");
  Assert.ok(url.slice(result.termsOffset).startsWith("caff%C3%A8"));

  // Decoding search terms with multiple spaces should work.
  result = Services.search.parseSubmissionURL(
             "http://www.google.com/search?q=+with++spaces+");
  Assert.equal(result.engine, engine1);
  Assert.equal(result.terms, " with  spaces ");

  // An empty query parameter should work the same.
  url = "http://www.google.com/search?q=";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine1);
  Assert.equal(result.terms, "");
  Assert.equal(result.termsOffset, url.length);

  // There should be no match when the path is different.
  result = Services.search.parseSubmissionURL(
             "http://www.google.com/search/?q=test");
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");
  Assert.equal(result.termsOffset, -1);

  // There should be no match when the argument is different.
  result = Services.search.parseSubmissionURL(
             "http://www.google.com/search?q2=test");
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");
  Assert.equal(result.termsOffset, -1);

  // There should be no match for URIs that are not HTTP or HTTPS.
  result = Services.search.parseSubmissionURL(
             "file://localhost/search?q=test");
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");
  Assert.equal(result.termsOffset, -1);
});
