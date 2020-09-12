/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that OpenSearch engines are installed and set up correctly.
 *
 * Note: simple.xml, post.xml, suggestion.xml and suggestion-alternate.xml
 * all use different namespaces to reflect the possibitities that may be
 * installed.
 * mozilla-ns.xml uses the mozilla namespace.
 */

"use strict";

const tests = [
  {
    file: "simple.xml",
    name: "simple",
    description: "A small test engine",
    searchForm: "https://example.com/",
    searchUrl: "https://example.com/search?q=foo",
  },
  {
    file: "post.xml",
    name: "Post",
    description: "",
    // The POST method is not supported for `rel="searchform"` so we fallback
    // to the `SearchForm` url.
    searchForm: "http://engine-rel-searchform-post.xml/?search",
    searchUrl: "https://example.com/post",
    searchPostData: "searchterms=foo",
  },
  {
    file: "suggestion.xml",
    name: "suggestion",
    description: "A small engine with suggestions",
    searchForm: "http://engine-rel-searchform.xml/?search",
    searchUrl: "https://example.com/search?q=foo",
    suggestUrl: "https://example.com/suggest?suggestion=foo",
  },
  {
    file: "suggestion-alternate.xml",
    name: "suggestion-alternate",
    description: "A small engine with suggestions",
    searchForm: "https://example.com/",
    searchUrl: "https://example.com/search?q=foo",
    suggestUrl: "https://example.com/suggest?suggestion=foo",
  },
  {
    file: "mozilla-ns.xml",
    name: "mozilla-ns",
    description: "An engine using mozilla namespace",
    searchForm: "https://example.com/",
    // mozilla-ns.xml also specifies a MozParam. However, they are only
    // valid for app-provided engines, and hence the param should not show
    // here.
    searchUrl: "https://example.com/search?q=foo",
  },
];

add_task(async function setup() {
  useHttpServer("opensearch");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

for (const test of tests) {
  add_task(async () => {
    info(`Testing ${test.file}`);
    let promiseEngineAdded = SearchTestUtils.promiseSearchNotification(
      SearchUtils.MODIFIED_TYPE.ADDED,
      SearchUtils.TOPIC_ENGINE_MODIFIED
    );
    let engine = await Services.search.addOpenSearchEngine(
      gDataUrl + test.file,
      null
    );
    await promiseEngineAdded;
    Assert.ok(engine, "Should have installed the engine.");

    Assert.equal(engine.name, test.name, "Should have the correct name");
    Assert.equal(
      engine.description,
      test.description,
      "Should have a description"
    );
    let submission = engine.getSubmission("foo");
    Assert.equal(
      submission.uri.spec,
      test.searchUrl,
      "Should have the correct search url"
    );

    if (test.searchPostData) {
      let sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
        Ci.nsIScriptableInputStream
      );
      sis.init(submission.postData);
      let data = sis.read(submission.postData.available());
      Assert.equal(
        decodeURIComponent(data),
        test.searchPostData,
        "Should have received the correct POST data"
      );
    } else {
      Assert.equal(
        submission.postData,
        null,
        "Should have not received any POST data"
      );
    }

    Assert.equal(
      engine.searchForm,
      test.searchForm,
      "Should have the correct search form url"
    );

    submission = engine.getSubmission("foo", SearchUtils.URL_TYPE.SUGGEST_JSON);
    if (test.suggestUrl) {
      Assert.equal(
        submission.uri.spec,
        test.suggestUrl,
        "Should have the correct suggest url"
      );
    } else {
      Assert.equal(submission, null, "Should not have a suggestion url");
    }
  });
}
