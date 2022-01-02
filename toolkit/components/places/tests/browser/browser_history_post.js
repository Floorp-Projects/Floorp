const PAGE_URI =
  "http://example.com/tests/toolkit/components/places/tests/browser/history_post.html";
const SJS_URI = NetUtil.newURI(
  "http://example.com/tests/toolkit/components/places/tests/browser/history_post.sjs"
);

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: PAGE_URI }, async function(
    aBrowser
  ) {
    await SpecialPowers.spawn(aBrowser, [], async function() {
      let doc = content.document;
      let submit = doc.getElementById("submit");
      let iframe = doc.getElementById("post_iframe");
      let p = new Promise((resolve, reject) => {
        iframe.addEventListener(
          "load",
          function() {
            resolve();
          },
          { once: true }
        );
      });
      submit.click();
      await p;
    });
    let visited = await PlacesUtils.history.hasVisits(SJS_URI);
    ok(!visited, "The POST page should not be added to history");
    ok(
      !(await PlacesTestUtils.isPageInDB(SJS_URI.spec)),
      "The page should not be in the database"
    );
  });
});
