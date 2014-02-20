const source = "http://example.com/browser/toolkit/components/viewsource/test/browser/file_bug464222.html";

function test() {
  waitForExplicitFinish();
  testSelection();
}

function testSelection() {
  openDocumentSelect(source, "a", function(aWindow) {
    let aTags = aWindow.gBrowser.contentDocument.querySelectorAll("a[href]");
    is(aTags[0].href, "view-source:" + source, "Relative links broken?");
    closeViewSourceWindow(aWindow, finish);
  });
}
