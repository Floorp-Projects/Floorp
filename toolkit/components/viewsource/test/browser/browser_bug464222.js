const source =
  "http://example.com/browser/toolkit/components/viewsource/test/browser/file_bug464222.html";

add_task(async function() {
  let viewSourceTab = await openDocumentSelect(source, "a");

  let href = await SpecialPowers.spawn(
    viewSourceTab.linkedBrowser,
    [],
    async function() {
      return content.document.querySelectorAll("a[href]")[0].href;
    }
  );

  is(href, "view-source:" + source, "Relative links broken?");
  gBrowser.removeTab(viewSourceTab);
});
