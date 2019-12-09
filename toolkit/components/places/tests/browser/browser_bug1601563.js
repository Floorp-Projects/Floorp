const PREFIX =
  "http://example.com/tests/toolkit/components/places/tests/browser/1601563";

function titleUpdate(pageUrl) {
  let lastTitle = null;
  return PlacesTestUtils.waitForNotification(
    "onTitleChanged",
    (uri, title) => {
      if (pageUrl != uri.spec) {
        return false;
      }
      lastTitle = title;
      return true;
    },
    "history"
  ).then(() => {
    return lastTitle;
  });
}

add_task(async function() {
  registerCleanupFunction(PlacesUtils.history.clear);
  const FIRST_URL = PREFIX + "-1.html";
  const SECOND_URL = PREFIX + "-2.html";
  let firstTitlePromise = titleUpdate(FIRST_URL);
  let secondTitlePromise = titleUpdate(SECOND_URL);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FIRST_URL);

  let firstTitle = await firstTitlePromise;
  is(firstTitle, "First title", "First title should match the page");

  let secondTitle = await secondTitlePromise;
  is(secondTitle, "Second title", "Second title should match the page");

  let entry = await PlacesUtils.history.fetch(FIRST_URL);
  is(
    entry.title,
    firstTitle,
    "Should not override first title with document.open()ed frame"
  );

  await BrowserTestUtils.removeTab(tab);
});
