/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  await SearchTestUtils.useTestEngines("method-extensions");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_get_extension() {
  let engine = Services.search.getEngineByName("Get Engine");
  Assert.notEqual(engine, null, "Should have found an engine");

  let url = engine.wrappedJSObject._getURLOfType(SearchUtils.URL_TYPE.SEARCH);
  Assert.equal(url.method, "GET", "Search URLs method is GET");

  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/?config=1&search=foo",
    "Search URLs should match"
  );

  let submissionSuggest = engine.getSubmission(
    "bar",
    SearchUtils.URL_TYPE.SUGGEST_JSON
  );
  Assert.equal(
    submissionSuggest.uri.spec,
    "https://example.com/?config=1&suggest=bar",
    "Suggest URLs should match"
  );
});

add_task(async function test_post_extension() {
  let engine = Services.search.getEngineByName("Post Engine");
  Assert.ok(!!engine, "Should have found an engine");

  let url = engine.wrappedJSObject._getURLOfType(SearchUtils.URL_TYPE.SEARCH);
  Assert.equal(url.method, "POST", "Search URLs method is POST");

  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/",
    "Search URLs should match"
  );
  Assert.equal(
    submission.postData.data.data,
    "config=1&search=foo",
    "Search postData should match"
  );

  let submissionSuggest = engine.getSubmission(
    "bar",
    SearchUtils.URL_TYPE.SUGGEST_JSON
  );
  Assert.equal(
    submissionSuggest.uri.spec,
    "https://example.com/",
    "Suggest URLs should match"
  );
  Assert.equal(
    submissionSuggest.postData.data.data,
    "config=1&suggest=bar",
    "Suggest postData should match"
  );
});
