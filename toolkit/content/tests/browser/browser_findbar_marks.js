/* eslint-disable mozilla/no-arbitrary-setTimeout */

// This test verifies that the find scrollbar marks are triggered in the right locations.
// Reftests in layout/xul/reftest are used to verify their appearance.

const TEST_PAGE_URI =
  "data:text/html,<body style='font-size: 20px; margin: 0;'><p style='margin: 0; height: 30px;'>This is some fun text.</p><p style='margin-top: 2000px; height: 30px;'>This is some tex to find.</p><p style='margin-top: 500px; height: 30px;'>This is some text to find.</p></body>";

add_task(async function test_findmarks() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );

  // Open the findbar so that the content scroll size can be measured.
  await promiseFindFinished(gBrowser, "s");

  let browser = tab.linkedBrowser;
  let scrollMaxY = await SpecialPowers.spawn(browser, [], () => {
    return content.scrollMaxY;
  });

  browser.sendMessageToActor(
    "Finder:EnableMarkTesting",
    { enable: true },
    "Finder"
  );

  let checkFn = event => {
    event.target.lastMarks = event.detail;
    event.target.eventsCount = event.target.eventsCount
      ? event.target.eventsCount + 1
      : 1;
    return false;
  };

  let endFn = BrowserTestUtils.addContentEventListener(
    browser,
    "find-scrollmarks-changed",
    () => {},
    { capture: true },
    checkFn
  );

  // For the first value, get the numbers and ensure that they are approximately
  // in the right place. Later tests should give the same values.
  await promiseFindFinished(gBrowser, "tex", true);

  // The exact values vary on each platform, so use a fuzzy match.
  let scrollVar = scrollMaxY - 1670;
  let values = await getMarks(browser, 1);
  SimpleTest.isfuzzy(values[0], 8, 3, "first value");
  SimpleTest.isfuzzy(values[1], 1305 + scrollVar, 10, "second value");
  SimpleTest.isfuzzy(values[2], 1650 + scrollVar, 10, "third value");

  await doAndVerifyFind(browser, "text", 2, [values[0], values[2]]);
  await doAndVerifyFind(browser, "", 3, []);
  await doAndVerifyFind(browser, "isz", 3, []); // marks should not be updated here
  await doAndVerifyFind(browser, "tex", 4, values);
  await doAndVerifyFind(browser, "isz", 5, []);
  await doAndVerifyFind(browser, "tex", 6, values);

  let findbar = await gBrowser.getFindBar();
  let closedPromise = BrowserTestUtils.waitForEvent(findbar, "findbarclose");
  await EventUtils.synthesizeKey("KEY_Escape");
  await closedPromise;

  await verifyFind(browser, "", 7, []);

  endFn();

  browser.sendMessageToActor(
    "Finder:EnableMarkTesting",
    { enable: false },
    "Finder"
  );

  gBrowser.removeTab(tab);
});

function getMarks(browser, expectedEventsCount) {
  return SpecialPowers.spawn(
    browser,
    [expectedEventsCount],
    expectedEventsCountChild => {
      Assert.equal(
        content.eventsCount,
        expectedEventsCountChild,
        "expected events count"
      );
      let marks = content.lastMarks;
      content.lastMarks = null;
      return marks || [];
    }
  );
}

async function doAndVerifyFind(
  browser,
  text,
  expectedEventsCount,
  expectedMarks
) {
  await promiseFindFinished(gBrowser, text, true);
  return verifyFind(browser, text, expectedEventsCount, expectedMarks);
}

async function verifyFind(browser, text, expectedEventsCount, expectedMarks) {
  let foundMarks = await getMarks(browser, expectedEventsCount);

  is(foundMarks.length, expectedMarks.length, "marks count with text " + text);
  for (let t = 0; t < foundMarks.length; t++) {
    SimpleTest.isfuzzy(
      foundMarks[t],
      expectedMarks[t],
      5,
      "mark " + t + " with text " + text
    );
  }

  Assert.deepEqual(foundMarks, expectedMarks, "basic find with text " + text);
}
