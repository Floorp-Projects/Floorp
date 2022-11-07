/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function setup_tab(url) {
  info(`Setting up ${url}`);
  let tabContent = BrowserTestUtils.addTab(gBrowser, url);

  await BrowserTestUtils.browserLoaded(tabContent.linkedBrowser);

  // For some of these tests we have to wait for the test to consume some
  // computation or memory.
  await SpecialPowers.spawn(tabContent.linkedBrowser, [], async () => {
    await content.wrappedJSObject.waitForTestReady();
  });

  return tabContent;
}

async function setup_about_performance() {
  info("Setting up about:performance");
  let tabAboutPerformance = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:performance"
  ));

  await BrowserTestUtils.browserLoaded(tabAboutPerformance.linkedBrowser);

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

  return {
    tab: tabAboutPerformance,
    doc,
    tbody,
  };
}

function find_row(tbody, title, tab) {
  // Find the row for our test tab.
  let row = tbody.firstChild;
  while (row && row.firstChild.textContent != title) {
    row = row.nextSibling;
  }

  Assert.ok(row, "found a table row for our test tab");
  Assert.equal(
    row.windowId,
    tab.linkedBrowser.outerWindowID,
    "the correct window id is set"
  );

  return row;
}

function checkEnergyMedHigh(row) {
  let l10nId = row.children[2].getAttribute("data-l10n-id");
  Assert.ok(
    ["energy-impact-medium", "energy-impact-high"].includes(l10nId),
    "our test tab is medium or high energy impact"
  );
}

async function checkMemoryAtLeast(bytes, row) {
  let memCell = row.children[3];
  ok(memCell, "Found the cell containing the amount of memory");

  if (!memCell.innerText) {
    info("There's no text yet, wait for an update");
    await new Promise(resolve => {
      let observer = new row.ownerDocument.ownerGlobal.MutationObserver(() => {
        observer.disconnect();
        resolve();
      });
      observer.observe(memCell, { childList: true });
    });
  }

  let text = memCell.innerText;
  ok(text, "Found the text from the memory cell");
  // We only bother to work in Megabytes, there's currently no reason to
  // make this more complex.
  info(`Text is ${text}.`);
  let mbStr = text.match(/^(\d+(\.\d+)?) MB$/);
  ok(mbStr && mbStr[1], "Matched a memory size in Megabytes");
  if (!mbStr) {
    return;
  }

  ok(bytes < Number(mbStr[1]) * 1024 * 1024, "Memory usage is high enough");
}

// Test that we can select the row for a tab and close it using the close
// button.
add_task(async function test_tab_operations() {
  let tabContent = await setup_tab(
    "http://example.com/browser/toolkit/components/aboutperformance/tests/browser/browser_compartments.html?test=" +
      Math.random()
  );

  let aboutPerformance = await setup_about_performance();

  // Find the row corresponding to our tab.
  let row = find_row(
    aboutPerformance.tbody,
    "Main frame for test browser_aboutperformance.js",
    tabContent
  );

  // Verify selecting a row works.
  EventUtils.synthesizeMouseAtCenter(
    row,
    {},
    aboutPerformance.tab.linkedBrowser.contentWindow
  );

  Assert.equal(
    row.getAttribute("selected"),
    "true",
    "doing a single click selects the row"
  );

  // Verify selecting a tab with a double click.
  Assert.equal(
    gBrowser.selectedTab,
    aboutPerformance.tab,
    "the about:performance tab is selected"
  );
  EventUtils.synthesizeMouseAtCenter(
    row,
    { clickCount: 2 },
    aboutPerformance.tab.linkedBrowser.contentWindow
  );
  Assert.equal(
    gBrowser.selectedTab,
    tabContent,
    "after a double click the test tab is selected"
  );

  info("Verify we can toggle subitems using a twisty image button");

  // Find the row with subtitems for twisty toggle test group.
  let twistyBtn = aboutPerformance.doc.querySelector("tr > td.root > .twisty");

  // When "toolkit.aboutPerformance.showInternals=false", there is no Twisty.
  if (
    Services.prefs.getBoolPref("toolkit.aboutPerformance.showInternals", false)
  ) {
    Assert.ok(twistyBtn, "A twisty button was found");
    let groupRow = twistyBtn.parentNode.parentNode;

    // Verify twisty button is properly set up.
    Assert.ok(
      twistyBtn.hasAttribute("aria-label"),
      "the Twisty image button has an aria-label"
    );
    Assert.equal(
      twistyBtn.getAttribute("aria-label"),
      groupRow.firstChild.textContent,
      "the Twisty image button's aria-label is the same as the Name of its row"
    );
    Assert.equal(
      twistyBtn.getAttribute("role"),
      "button",
      "the Twisty image is programmatically a button"
    );
    Assert.equal(
      twistyBtn.getAttribute("tabindex"),
      "0",
      "the Twisty image button is included in the focus order"
    );
    Assert.equal(
      twistyBtn.getAttribute("aria-expanded"),
      "false",
      "the Twisty image button is collapsed by default"
    );

    // Verify we can toggle/show subitems by clicking the twisty button.
    EventUtils.synthesizeMouseAtCenter(
      twistyBtn,
      {},
      aboutPerformance.tab.linkedBrowser.contentWindow
    );
    Assert.ok(
      groupRow.nextSibling.children[0].classList.contains("indent"),
      "clicking a collapsed Twisty adds subitems after the row"
    );
    Assert.equal(
      twistyBtn.getAttribute("aria-expanded"),
      "true",
      "the Twisty image button is expanded after a click"
    );

    // Verify the twisty button can be focused with a keyboard.
    twistyBtn.focus();
    Assert.equal(
      twistyBtn,
      aboutPerformance.doc.activeElement,
      "the Twisty image button can be focused"
    );
    // Verify we can toggle subitems with a keyboard.
    // Twisty is expanded
    EventUtils.synthesizeKey(
      "KEY_Enter",
      {},
      aboutPerformance.tab.linkedBrowser.contentWindow
    );
    Assert.ok(
      !groupRow.nextSibling ||
        !groupRow.nextSibling.children[0].classList.contains("indent"),
      "pressing Enter on expanded Twisty removes subitems after the row"
    );
    Assert.equal(
      twistyBtn.getAttribute("aria-expanded"),
      "false",
      "the Twisty image button is collapsed after a keypress"
    );
    Assert.equal(
      twistyBtn,
      aboutPerformance.doc.activeElement,
      "the Twisty retains focus after the page is updated"
    );
    // Twisty is collapsed
    EventUtils.synthesizeKey(
      " ",
      {},
      aboutPerformance.tab.linkedBrowser.contentWindow
    );
    Assert.ok(
      groupRow.nextSibling.children[0].classList.contains("indent"),
      "pressing Space on collapsed Twisty adds subitems after the row"
    );
    Assert.equal(
      twistyBtn.getAttribute("aria-expanded"),
      "true",
      "the Twisty image button is expanded after a keypress"
    );

    info("Verify the focus stays on a twisty image button");

    Assert.equal(
      twistyBtn,
      aboutPerformance.doc.activeElement,
      "the Twisty retains focus after the page is updated"
    );
    Assert.notEqual(
      aboutPerformance.doc.activeElement.tagName,
      "body",
      "the body does not pull the focus after the page is updated"
    );
    EventUtils.synthesizeKey(
      "KEY_Tab",
      { shiftKey: true },
      aboutPerformance.tab.linkedBrowser.contentWindow
    );
    Assert.notEqual(
      twistyBtn,
      aboutPerformance.doc.activeElement,
      "the Twisty does not pull the focus after the page is updated"
    );
  } else {
    Assert.ok(
      !twistyBtn,
      "No twisty button should exist when the showInternals pref is false"
    );
  }

  info("Verify we can close a tab using the X button");
  // Switch back to about:performance...
  await BrowserTestUtils.switchTab(gBrowser, aboutPerformance.tab);
  // ... and click the X button at the end of the row.
  let tabClosing = BrowserTestUtils.waitForTabClosing(tabContent);
  EventUtils.synthesizeMouseAtCenter(
    row.children[4],
    {},
    aboutPerformance.tab.linkedBrowser.contentWindow
  );
  await tabClosing;

  BrowserTestUtils.removeTab(aboutPerformance.tab);
});

add_task(async function test_tab_energy() {
  let tabContent = await setup_tab(
    "http://example.com/browser/toolkit/components/aboutperformance/tests/browser/browser_compartments.html?test=" +
      Math.random()
  );

  let aboutPerformance = await setup_about_performance();

  // Find the row corresponding to our tab.
  let row = find_row(
    aboutPerformance.tbody,
    "Main frame for test browser_aboutperformance.js",
    tabContent
  );

  // Ensure it is reported as a medium or high energy impact.
  checkEnergyMedHigh(row);

  await BrowserTestUtils.removeTab(tabContent);
  await BrowserTestUtils.removeTab(aboutPerformance.tab);
});

add_task(async function test_tab_memory() {
  let tabContent = await setup_tab(
    "http://example.com/browser/toolkit/components/aboutperformance/tests/browser/tab_use_memory.html"
  );

  let aboutPerformance = await setup_about_performance();

  // Find the row corresponding to our tab.
  let row = find_row(
    aboutPerformance.tbody,
    "Main frame for test browser_aboutperformance.js",
    tabContent
  );

  // The page is using at least 32 MB, due to the big array that it
  // contains.
  await checkMemoryAtLeast(32 * 1024 * 1024, row);

  await BrowserTestUtils.removeTab(tabContent);
  await BrowserTestUtils.removeTab(aboutPerformance.tab);
});

add_task(async function test_worker_energy() {
  let tabContent = await setup_tab(
    "http://example.com/browser/toolkit/components/aboutperformance/tests/browser/workers.html"
  );

  let aboutPerformance = await setup_about_performance();

  // Find the row corresponding to our tab.
  let row = find_row(
    aboutPerformance.tbody,
    "Main frame for test browser_aboutperformance.js",
    tabContent
  );

  // Find the worker under this row.
  let button = row.firstChild.firstChild;
  Assert.ok(button && button.classList, "Has a span to create the button");
  Assert.ok(button.classList.contains("twisty"), "Button is expandable.");
  Assert.ok(!button.classList.contains("open"), "Not already open");

  // Click the expand button.
  EventUtils.synthesizeMouseAtCenter(
    button,
    {},
    aboutPerformance.tab.linkedBrowser.contentWindow
  );

  Assert.ok(button.classList.contains("open"), "It's now open");

  // Move to the next row which is the worker we want to imspect.
  row = row.nextSibling;

  // Check that it is a worker.
  Assert.equal(row.children[1].getAttribute("data-l10n-id"), "type-worker");

  // Ensure it is reported as a medium or high energy impact.
  checkEnergyMedHigh(row);

  await BrowserTestUtils.removeTab(tabContent);
  await BrowserTestUtils.removeTab(aboutPerformance.tab);
});

add_task(async function test_worker_memory() {
  let tabContent = await setup_tab(
    "http://example.com/browser/toolkit/components/aboutperformance/tests/browser/workers_memory.html"
  );

  let aboutPerformance = await setup_about_performance();

  // Find the row corresponding to our tab.
  let row = find_row(
    aboutPerformance.tbody,
    "Main frame for test browser_aboutperformance.js",
    tabContent
  );
  Assert.ok(row, "Found the row for our test tab");

  // Find the worker under this row.
  let button = row.firstChild.firstChild;
  Assert.ok(button && button.classList, "Has a span to create the button");
  Assert.ok(button.classList.contains("twisty"), "Button is expandable.");
  Assert.ok(!button.classList.contains("open"), "Not already open");

  // Click the expand button.
  EventUtils.synthesizeMouseAtCenter(
    button,
    {},
    aboutPerformance.tab.linkedBrowser.contentWindow
  );

  Assert.ok(button.classList.contains("open"), "It's now open");

  // Move to the next row which is the worker we want to imspect.
  row = row.nextSibling;

  // Check that it is a worker.
  Assert.equal(row.children[1].getAttribute("data-l10n-id"), "type-worker");

  // The page is using at least 32 MB, due to the big array that it
  // contains.
  await checkMemoryAtLeast(32 * 1024 * 1024, row);

  await BrowserTestUtils.removeTab(tabContent);
  await BrowserTestUtils.removeTab(aboutPerformance.tab);
});
