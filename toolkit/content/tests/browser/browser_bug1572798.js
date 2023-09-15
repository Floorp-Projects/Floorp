add_task(async function test_bug_1572798() {
  let tab = BrowserTestUtils.addTab(window.gBrowser, "about:blank");
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "https://example.com/browser/toolkit/content/tests/browser/file_document_open_audio.html"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  let windowLoaded = BrowserTestUtils.waitForNewWindow();
  info("- clicking button to spawn a new window -");
  await ContentTask.spawn(tab.linkedBrowser, null, function () {
    content.document.querySelector("button").click();
  });
  info("- waiting for the new window -");
  let newWin = await windowLoaded;
  info("- checking that the new window plays the audio -");
  let documentOpenedBrowser = newWin.gBrowser.selectedBrowser;
  await ContentTask.spawn(documentOpenedBrowser, null, async function () {
    try {
      await content.document.querySelector("audio").play();
      ok(true, "Could play the audio");
    } catch (e) {
      ok(false, "Rejected audio promise" + e);
    }
  });

  info("- Cleaning up -");
  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.removeTab(tab);
});
