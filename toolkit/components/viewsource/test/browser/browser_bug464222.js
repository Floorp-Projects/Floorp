const source = "http://example.com/browser/toolkit/components/viewsource/test/browser/file_bug464222.html";

add_task(function *() {
  let viewSourceTab = yield* openDocumentSelect(source, "a");

  let href = yield ContentTask.spawn(viewSourceTab.linkedBrowser, { }, function* () {
    return content.document.querySelectorAll("a[href]")[0].href;
  });

  is(href, "view-source:" + source, "Relative links broken?");
  gBrowser.removeTab(viewSourceTab);
});
