/* Any copyright is dedicated to the Public Domain.
 *    https://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getAlternateDomains API.
 */

"use strict";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_parseSubmissionURL() {
  let engine1 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine.xml`,
  });
  let engine2 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine-fr.xml`,
  });

  await SearchTestUtils.installSearchExtension({
    name: "bacon_addParam",
    keyword: "bacon_addParam",
    encoding: "windows-1252",
    search_url: "https://www.bacon.test/find",
  });
  await SearchTestUtils.installSearchExtension({
    name: "idn_addParam",
    keyword: "idn_addParam",
    search_url: "https://www.xn--bcher-kva.ch/search",
  });
  let engine3 = Services.search.getEngineByName("bacon_addParam");
  let engine4 = Services.search.getEngineByName("idn_addParam");

  // The following engine provides it's query keyword in
  // its template in the form of q={searchTerms}
  let engine5 = await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine2.xml`,
  });

  // The following engines cannot identify the search parameter.
  await SearchTestUtils.installSearchExtension({
    name: "bacon",
    keyword: "bacon",
    search_url: "https://www.bacon.moz/search?q=",
    search_url_get_params: "",
  });

  await Services.search.setDefault(
    engine1,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  // Hide the default engines to prevent them from being used in the search.
  for (let engine of await Services.search.getAppProvidedEngines()) {
    await Services.search.removeEngine(engine);
  }

  // Test the first engine, whose URLs use UTF-8 encoding.
  // This also tests the query parameter in a different position not being the
  // first parameter.
  let url = "https://www.google.com/search?foo=bar&q=caff%C3%A8";
  let result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine.wrappedJSObject, engine1);
  Assert.equal(result.terms, "caff\u00E8");

  // The second engine uses a locale-specific domain that is an alternate domain
  // of the first one, but the second engine should get priority when matching.
  // The URL used with this engine uses ISO-8859-1 encoding instead.
  url = "https://www.google.fr/search?q=caff%E8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine.wrappedJSObject, engine2);
  Assert.equal(result.terms, "caff\u00E8");

  // Test a domain that is an alternate domain of those defined.  In this case,
  // the first matching engine from the ordered list should be returned.
  url = "https://www.google.co.uk/search?q=caff%C3%A8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine.wrappedJSObject, engine1);
  Assert.equal(result.terms, "caff\u00E8");

  // We support parsing URLs from a dynamically added engine.
  url = "https://www.bacon.test/find?q=caff%E8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine3);
  Assert.equal(result.terms, "caff\u00E8");

  // Test URLs with unescaped unicode characters.
  url = "https://www.google.com/search?q=foo+b\u00E4r";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine.wrappedJSObject, engine1);
  Assert.equal(result.terms, "foo b\u00E4r");

  // Test search engines with unescaped IDNs.
  url = "https://www.b\u00FCcher.ch/search?q=foo+bar";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine4);
  Assert.equal(result.terms, "foo bar");

  // Test search engines with escaped IDNs.
  url = "https://www.xn--bcher-kva.ch/search?q=foo+bar";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine, engine4);
  Assert.equal(result.terms, "foo bar");

  // Parsing of parameters from an engine template URL is not supported
  // if no matching parameter value template is provided.
  Assert.equal(
    Services.search.parseSubmissionURL("https://www.bacon.moz/search?q=")
      .engine,
    null
  );

  // Parsing of parameters from an engine template URL is supported
  // if a matching parameter value template is provided.
  url = "https://duckduckgo.com/?foo=bar&q=caff%C3%A8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine.wrappedJSObject, engine5);
  Assert.equal(result.terms, "caff\u00E8");

  // If the search params are in the template, the query parameter
  // doesn't need to be separated from the host by a slash, only by
  // by a question mark.
  url = "https://duckduckgo.com?foo=bar&q=caff%C3%A8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine.wrappedJSObject, engine5);
  Assert.equal(result.terms, "caff\u00E8");

  // HTTP and HTTPS schemes are interchangeable.
  url = "https://www.google.com/search?q=caff%C3%A8";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine.wrappedJSObject, engine1);
  Assert.equal(result.terms, "caff\u00E8");

  // Decoding search terms with multiple spaces should work.
  result = Services.search.parseSubmissionURL(
    "https://www.google.com/search?q=+with++spaces+"
  );
  Assert.equal(result.engine.wrappedJSObject, engine1);
  Assert.equal(result.terms, " with  spaces ");

  // Parsing search terms with ampersands should work.
  result = Services.search.parseSubmissionURL(
    "https://www.google.com/search?q=with%26ampersand"
  );
  Assert.equal(result.engine.wrappedJSObject, engine1);
  Assert.equal(result.terms, "with&ampersand");

  // Capitals in the path should work
  result = Services.search.parseSubmissionURL(
    "https://www.google.com/SEARCH?q=caps"
  );
  Assert.equal(result.engine.wrappedJSObject, engine1);
  Assert.equal(result.terms, "caps");

  // An empty query parameter should work the same.
  url = "https://www.google.com/search?q=";
  result = Services.search.parseSubmissionURL(url);
  Assert.equal(result.engine.wrappedJSObject, engine1);
  Assert.equal(result.terms, "");

  // There should be no match when the path is different.
  result = Services.search.parseSubmissionURL(
    "https://www.google.com/search/?q=test"
  );
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");

  // There should be no match when the argument is different.
  result = Services.search.parseSubmissionURL(
    "https://www.google.com/search?q2=test"
  );
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");

  // There should be no match for URIs that are not HTTP or HTTPS.
  result = Services.search.parseSubmissionURL("file://localhost/search?q=test");
  Assert.equal(result.engine, null);
  Assert.equal(result.terms, "");
});
