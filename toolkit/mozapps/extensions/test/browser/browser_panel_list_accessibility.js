/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function setupPanel(win) {
  let doc = win.document;
  // Clear out the other elements so only our test content is on the page.
  doc.body.textContent = "";
  let panelList = doc.createElement("panel-list");
  let items = ["one", "two", "three"];
  let panelItems = items.map(item => {
    let panelItem = doc.createElement("panel-item");
    panelItem.textContent = item;
    panelList.append(panelItem);
    return panelItem;
  });

  let anchorButton = doc.createElement("button");
  anchorButton.addEventListener("click", e => panelList.toggle(e));

  // Insert the button at the top of the page so it's in view.
  doc.body.prepend(anchorButton, panelList);

  return { anchorButton, panelList, panelItems };
}

add_task(async function testItemFocusOnOpen() {
  let win = await loadInitialView("extension");
  let doc = win.document;

  let { anchorButton, panelList, panelItems } = setupPanel(win);

  ok(doc.activeElement, "There is an active element");
  ok(!doc.activeElement.closest("panel-list"), "Focus isn't in the list");

  let shown = BrowserTestUtils.waitForEvent(panelList, "shown");
  EventUtils.synthesizeMouseAtCenter(anchorButton, {}, win);
  await shown;

  is(doc.activeElement, panelItems[0], "The first item is focused");

  let hidden = BrowserTestUtils.waitForEvent(panelList, "hidden");
  EventUtils.synthesizeKey("Escape", {}, win);
  await hidden;

  is(doc.activeElement, anchorButton, "The anchor is focused again on close");

  await closeView(win);
});

add_task(async function testAriaAttributes() {
  let win = await loadInitialView("extension");

  let { panelList, panelItems } = setupPanel(win);

  is(panelList.getAttribute("role"), "menu", "The panel is a menu");

  is(panelItems.length, 3, "There are 3 items");
  Assert.deepEqual(
    panelItems.map(panelItem => panelItem.button.getAttribute("role")),
    new Array(panelItems.length).fill("menuitem"),
    "All of the items have a menuitem button"
  );

  await closeView(win);
});
