/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL =
  "http://example.com/browser/toolkit/components/aboutperformance/tests/browser/browser_compartments.html?test=" +
  Math.random();

add_task(async function init() {
  info("Setting up about:performance");
  let tabAboutPerformance = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:performance"
  ));

  await BrowserTestUtils.browserLoaded(tabAboutPerformance.linkedBrowser);

  info(`Setting up ${URL}`);
  let tabContent = BrowserTestUtils.addTab(gBrowser, URL);
  await BrowserTestUtils.browserLoaded(tabContent.linkedBrowser);

  let doc = tabAboutPerformance.linkedBrowser.contentDocument;
  let tbody = doc.getElementById("dispatch-tbody");

  // Wait until the table has first been populated.
  await TestUtils.waitForCondition(() => tbody.childElementCount);

  // And wait for another update using a mutation observer, to give our newly created test tab some time
  // to burn some CPU.
  await new Promise(resolve => {
    let observer = new doc.ownerGlobal.MutationObserver(() => {
      observer.disconnect();
      resolve();
    });
    observer.observe(tbody, { childList: true });
  });

  // Find the row for our test tab.
  let row = tbody.firstChild;
  while (
    row &&
    row.firstChild.textContent !=
      "Main frame for test browser_aboutperformance.js"
  ) {
    row = row.nextSibling;
  }

  Assert.ok(row, "found a table row for our test tab");
  Assert.equal(
    row.windowId,
    tabContent.linkedBrowser.outerWindowID,
    "the correct window id is set"
  );

  // Ensure it is reported as a medium or high energy impact.
  let l10nId = row.children[2].getAttribute("data-l10n-id");
  Assert.ok(
    ["energy-impact-medium", "energy-impact-high"].includes(l10nId),
    "our test tab is medium or high energy impact"
  );

  // Verify selecting a row works.
  EventUtils.synthesizeMouseAtCenter(
    row,
    {},
    tabAboutPerformance.linkedBrowser.contentWindow
  );

  Assert.equal(
    row.getAttribute("selected"),
    "true",
    "doing a single click selects the row"
  );

  // Verify selecting a tab with a double click.
  Assert.equal(
    gBrowser.selectedTab,
    tabAboutPerformance,
    "the about:performance tab is selected"
  );
  EventUtils.synthesizeMouseAtCenter(
    row,
    { clickCount: 2 },
    tabAboutPerformance.linkedBrowser.contentWindow
  );
  Assert.equal(
    gBrowser.selectedTab,
    tabContent,
    "after a double click the test tab is selected"
  );

  // Verify we can close a tab using the X button.
  // Switch back to about:performance...
  await BrowserTestUtils.switchTab(gBrowser, tabAboutPerformance);
  // ... and click the X button at the end of the row.
  let tabClosing = BrowserTestUtils.waitForTabClosing(tabContent);
  EventUtils.synthesizeMouseAtCenter(
    row.children[4],
    {},
    tabAboutPerformance.linkedBrowser.contentWindow
  );
  await tabClosing;

  BrowserTestUtils.removeTab(tabAboutPerformance);
});
