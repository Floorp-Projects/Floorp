const PAGE_URI = "http://example.com/tests/toolkit/components/places/tests/browser/history_post.html";
const SJS_URI = NetUtil.newURI("http://example.com/tests/toolkit/components/places/tests/browser/history_post.sjs");

add_task(function* () {
  yield BrowserTestUtils.withNewTab({gBrowser, url: PAGE_URI}, Task.async(function* (aBrowser) {
    yield ContentTask.spawn(aBrowser, null, function* () {
      let doc = content.document;
      let submit = doc.getElementById("submit");
      let iframe = doc.getElementById("post_iframe");
      let p = new Promise((resolve, reject) => {
        iframe.addEventListener("load", function onLoad() {
          iframe.removeEventListener("load", onLoad);
          resolve();
        });
      });
      submit.click();
      yield p;
    });
    let visited = yield promiseIsURIVisited(SJS_URI);
    ok(!visited, "The POST page should not be added to history");
    let db = yield PlacesUtils.promiseDBConnection();
    let rows = yield db.execute(
      "SELECT 1 FROM moz_places WHERE url = :page_url",
      {page_url: SJS_URI.spec});
    is(rows.length, 0, "The page should not be in the database");
    yield db.close();
  }));
});
