/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function thumbnails_bug1775638() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  const canvas = document.createElement("canvas");
  const promise = PageThumbs.captureToCanvas(tab.linkedBrowser, canvas);
  gBrowser.removeTab(tab);
  is(await promise, canvas);
});
