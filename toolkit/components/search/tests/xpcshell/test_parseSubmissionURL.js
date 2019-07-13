/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getAlternateDomains API.
 */

"use strict";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_parseSubmissionURL() {
  // Hide the default engines to prevent them from being used in the search.
  for (let engine of await Services.search.getEngines()) {
    await Services.search.removeEngine(engine);
  }

  let engines = await addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "Test search engine (fr)", xmlFileName: "engine-fr.xml" },
    {
      name: "bacon_addParam",
      details: {
        alias: "bacon_addParam",
        description: "Search Bacon",
        method: "GET",
        template: "http://www.bacon.test/find",
      },
    },
    {
      name: "idn_addParam",
      details: {
        alias: "idn_addParam",
        description: "Search IDN",
        method: "GET",
        template: "http://www.xn--bcher-kva.ch/search",
      },
    },
    // The following engines cannot identify the search parameter.
    { name: "A second test engine", xmlFileName: "engine2.xml" },
    {
      name: "bacon",
      details: {
        alias: "bacon",
        description: "Search Bacon",
        method: "GET",
        template: "http://www.bacon.moz/search?q={searchTerms}",
      },
    },
  ]);

  engines[2].addParam("q", "{searchTerms}", null);
  engines[3].addParam("q", "{searchTerms}", null);

  function testParseSubmissionURL(url, engine, terms = "", offsetTerm) {
    let result = Services.search.parseSubmissionURL(url);
    Assert.equal(result.engine.name, engine.name, "engine matches");
    Assert.equal(result.terms, terms, "term matches");
    if (offsetTerm) {
      Assert.ok(
        url.slice(result.termsOffset).startsWith(offsetTerm),
        "offset term matches"
      );
      Assert.equal(
        result.termsLength,
        offsetTerm.length,
        "offset term length matches"
      );
    } else {
      Assert.equal(result.termsOffset, url.length, "no term offset");
    }
  }

  // Test the first engine, whose URLs use UTF-8 encoding.
  info("URLs use UTF-8 encoding");
  testParseSubmissionURL(
    "http://www.google.com/search?foo=bar&q=caff%C3%A8",
    engines[0],
    "caff\u00E8",
    "caff%C3%A8"
  );

  // The second engine uses a locale-specific domain that is an alternate domain
  // of the first one, but the second engine should get priority when matching.
  // The URL used with this engine uses ISO-8859-1 encoding instead.
  info("URLs use alternate domain and ISO-8859-1 encoding");
  testParseSubmissionURL(
    "http://www.google.fr/search?q=caff%E8",
    engines[1],
    "caff\u00E8",
    "caff%E8"
  );

  // Test a domain that is an alternate domain of those defined.  In this case,
  // the first matching engine from the ordered list should be returned.
  info("URLs use alternate domain");
  testParseSubmissionURL(
    "http://www.google.co.uk/search?q=caff%C3%A8",
    engines[0],
    "caff\u00E8",
    "caff%C3%A8"
  );

  // We support parsing URLs from a dynamically added engine.  Those engines use
  // windows-1252 encoding by default.
  info("URLs use windows-1252");
  testParseSubmissionURL(
    "http://www.bacon.test/find?q=caff%E8",
    engines[2],
    "caff\u00E8",
    "caff%E8"
  );

  info("URLs with unescaped unicode characters");
  testParseSubmissionURL(
    "http://www.google.com/search?q=foo+b\u00E4r",
    engines[0],
    "foo b\u00E4r",
    "foo+b\u00E4r"
  );

  info("URLs with unescaped IDNs");
  testParseSubmissionURL(
    "http://www.b\u00FCcher.ch/search?q=foo+bar",
    engines[3],
    "foo bar",
    "foo+bar"
  );

  info("URLs with escaped IDNs");
  testParseSubmissionURL(
    "http://www.xn--bcher-kva.ch/search?q=foo+bar",
    engines[3],
    "foo bar",
    "foo+bar"
  );

  info("URLs with engines using template params, no value");
  testParseSubmissionURL("http://www.bacon.moz/search?q=", engines[5]);

  info("URLs with engines using template params");
  testParseSubmissionURL(
    "https://duckduckgo.com?q=test",
    engines[4],
    "test",
    "test"
  );

  info("HTTP and HTTPS schemes are interchangeable.");
  testParseSubmissionURL(
    "https://www.google.com/search?q=caff%C3%A8",
    engines[0],
    "caff\u00E8",
    "caff%C3%A8"
  );

  info("Decoding search terms with multiple spaces should work.");
  testParseSubmissionURL(
    "http://www.google.com/search?q=+with++spaces+",
    engines[0],
    " with  spaces ",
    "+with++spaces+"
  );

  info("An empty query parameter should work the same.");
  testParseSubmissionURL("http://www.google.com/search?q=", engines[0]);

  // These test slightly different so we don't use testParseSubmissionURL.
  info("There should be no match when the path is different.");
  let result = Services.search.parseSubmissionURL(
    "http://www.google.com/search/?q=test"
  );
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");
  Assert.equal(result.termsOffset, -1);

  info("There should be no match when the argument is different.");
  result = Services.search.parseSubmissionURL(
    "http://www.google.com/search?q2=test"
  );
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");
  Assert.equal(result.termsOffset, -1);

  info("There should be no match for URIs that are not HTTP or HTTPS.");
  result = Services.search.parseSubmissionURL("file://localhost/search?q=test");
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");
  Assert.equal(result.termsOffset, -1);
});
