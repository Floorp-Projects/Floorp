/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 679604 - Test that a XUL persisted category from an older version of
// Firefox doesn't break the add-ons manager when that category doesn't exist

const PREF_UI_LASTCATEGORY = "extensions.ui.lastCategory";

var gManagerWindow;

function test() {
  waitForExplicitFinish();

  open_manager(null, function(aWindow) {
    var categories = aWindow.document.getElementById("categories");
    categories.setAttribute("last-selected", "foo");
    aWindow.document.persist("categories", "last-selected");

    close_manager(aWindow, function() {
      Services.prefs.clearUserPref(PREF_UI_LASTCATEGORY);

      open_manager(null, function(aWindow) {
        gCategoryUtilities = new CategoryUtilities(aWindow);
        is(gCategoryUtilities.selectedCategory, "discover", "Should have loaded the right view");

        close_manager(aWindow, finish);
      });
    });
  });
}
