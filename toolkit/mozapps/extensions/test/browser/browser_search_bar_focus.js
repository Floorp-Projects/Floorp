/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 570760 - Make ctrl-f and / focus the search box in the add-ons manager

function testKeys(win, searchBox) {
  let doc = win.document;
  doc.firstElementChild.focus();
  isnot(doc.activeElement, searchBox, "Search box is not focused");
  EventUtils.synthesizeKey("f", { accelKey: true }, win);
  is(
    searchBox.ownerDocument.activeElement,
    searchBox,
    "ctrl-f focuses search box"
  );

  searchBox.blur();

  doc.firstElementChild.focus();
  isnot(doc.activeElement, searchBox, "Search box is not focused");
  EventUtils.synthesizeKey("/", {}, win);
  is(searchBox.ownerDocument.activeElement, searchBox, "/ focuses search box");

  searchBox.blur();
}

add_task(async function testSearchBarKeyboardAccess() {
  let win = await loadInitialView("extension");

  let doc = win.document;
  let searchBox = doc.querySelector("search-addons").input;

  testKeys(win, searchBox);

  await closeView(win);
});
