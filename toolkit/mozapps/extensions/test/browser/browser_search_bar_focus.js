/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 570760 - Make ctrl-f and / focus the search box in the add-ons manager

async function testKeys(win, searchBox) {
  let doc = win.document;
  // If focus is on the search box move it away for the rest
  // of the test.
  if (doc.activeElement == searchBox) {
    let focus = BrowserTestUtils.waitForEvent(doc.firstElementChild, "focus");
    doc.firstElementChild.focus();
    await focus;
  }
  isnot(doc.activeElement, searchBox, "Search box is not focused");

  let focus = BrowserTestUtils.waitForEvent(searchBox, "focus");
  EventUtils.synthesizeKey("f", { accelKey: true }, win);
  await focus;
  is(
    searchBox.ownerDocument.activeElement,
    searchBox,
    "ctrl-f focuses search box"
  );

  let blur = BrowserTestUtils.waitForEvent(searchBox, "blur");
  searchBox.blur();

  doc.firstElementChild.focus();
  await blur;
  isnot(doc.activeElement, searchBox, "Search box is not focused");

  focus = BrowserTestUtils.waitForEvent(searchBox, "focus");
  EventUtils.synthesizeKey("/", {}, win);
  await focus;
  is(searchBox.ownerDocument.activeElement, searchBox, "/ focuses search box");

  searchBox.blur();
}

add_task(async function testSearchBarKeyboardAccess() {
  let win = await loadInitialView("extension");

  let doc = win.document;
  let searchBox = doc.querySelector("search-addons").input;

  await testKeys(win, searchBox);

  await closeView(win);
});
