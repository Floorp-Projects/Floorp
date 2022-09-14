/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let doc, tbody, tabAboutProcesses;

const rowTypes = ["process", "window", "thread-summary", "thread"];

function promiseUpdate() {
  return promiseAboutProcessesUpdated({
    doc,
    tbody,
    force: true,
    tabAboutProcesses,
  });
}

add_setup(async function() {
  Services.prefs.setBoolPref("toolkit.aboutProcesses.showThreads", true);

  info("Setting up about:processes");
  tabAboutProcesses = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:processes",
    waitForLoad: true,
  });

  doc = tabAboutProcesses.linkedBrowser.contentDocument;
  tbody = doc.getElementById("process-tbody");
  await promiseUpdate();
});

add_task(function testTwistyImageButtonSetup() {
  let twistyBtn = doc.querySelector("tr.thread-summary .twisty");
  let groupRow = twistyBtn.parentNode.parentNode;
  let groupRowId = groupRow.firstChild.children[1].getAttribute("id");
  let groupRowLabelId = groupRowId.split(":")[1];

  info("Verify twisty button is properly set up.");
  Assert.ok(
    twistyBtn.hasAttribute("aria-labelledby"),
    "the Twisty image button has an aria-labelledby"
  );
  Assert.equal(
    twistyBtn.getAttribute("aria-labelledby"),
    `${groupRowLabelId}-label ${groupRowId}`,
    "the Twisty image button's aria-labelledby refers to a valid 'id' that is the Name of its row"
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
});

add_task(function testTwistyImageButtonClicking() {
  let twistyBtn = doc.querySelector("tr.thread-summary .twisty");
  let groupRow = twistyBtn.parentNode.parentNode;

  info(
    "Verify we can toggle/open a list of threads by clicking the twisty button."
  );
  twistyBtn.click();

  Assert.ok(
    groupRow.nextSibling.classList.contains("thread") &&
      !groupRow.nextSibling.classList.contains("thread-summary"),
    "clicking a collapsed Twisty adds subitems after the row"
  );
  Assert.equal(
    twistyBtn.getAttribute("aria-expanded"),
    "true",
    "the Twisty image button is expanded after a click"
  );
});

add_task(function testTwistyImageButtonKeypressing() {
  let twistyBtn = doc.querySelector("tr.thread-summary .twisty");
  let groupRow = twistyBtn.parentNode.parentNode;

  info(
    `Verify we can toggle/close a list of threads by pressing Enter or
    Space on the twisty button.`
  );
  // Verify the twisty button can be focused with a keyboard.
  twistyBtn.focus();
  Assert.equal(
    twistyBtn,
    doc.activeElement,
    "the Twisty image button can be focused"
  );

  // Verify we can toggle subitems with a keyboard.
  // Twisty is expanded
  EventUtils.synthesizeKey("KEY_Enter");
  Assert.ok(
    !groupRow.nextSibling.classList.contains("thread") ||
      groupRow.nextSibling.classList.contains("thread-summary"),
    "pressing Enter on expanded Twisty hides a list of threads after the row"
  );
  Assert.equal(
    twistyBtn.getAttribute("aria-expanded"),
    "false",
    "the Twisty image button is collapsed after an Enter keypress"
  );
  // Twisty is collapsed
  EventUtils.synthesizeKey(" ");
  Assert.ok(
    groupRow.nextSibling.classList.contains("thread") &&
      !groupRow.nextSibling.classList.contains("thread-summary"),
    "pressing Space on collapsed Twisty shows a list of threads after the row"
  );
  Assert.equal(
    twistyBtn.getAttribute("aria-expanded"),
    "true",
    "the Twisty image button is expanded after a Space keypress"
  );
});

add_task(function cleanup() {
  BrowserTestUtils.removeTab(tabAboutProcesses);
  Services.prefs.clearUserPref("toolkit.aboutProcesses.showThreads");
});
