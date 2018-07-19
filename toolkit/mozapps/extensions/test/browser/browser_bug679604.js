/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 679604 - Test that a XUL persisted category from an older version of
// Firefox doesn't break the add-ons manager when that category doesn't exist

async function test() {
  waitForExplicitFinish();

  let aWindow = await open_manager(null);
  var categories = aWindow.document.getElementById("categories");
  categories.setAttribute("last-selected", "foo");
  Services.xulStore.persist(categories, "last-selected");

  await close_manager(aWindow);
  Services.prefs.clearUserPref(PREF_UI_LASTCATEGORY);

  aWindow = await open_manager(null);
  is(new CategoryUtilities(aWindow).selectedCategory, "discover",
     "Should have loaded the right view");

  close_manager(aWindow, finish);
}
