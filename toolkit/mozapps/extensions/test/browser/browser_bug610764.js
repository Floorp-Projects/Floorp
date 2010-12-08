/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the discovery view is the default

var gCategoryUtilities;

function test() {
  waitForExplicitFinish();

  open_manager(null, function(aWindow) {
    waitForFocus(function() {
      // The last view is cached except when it is the search view so switch to
      // that and reopen to ensure we see the default view
      var searchBox = aWindow.document.getElementById("header-search");
      searchBox.value = "bar";

      EventUtils.synthesizeMouseAtCenter(searchBox, { }, aWindow);
      EventUtils.synthesizeKey("VK_RETURN", { }, aWindow);

      wait_for_view_load(aWindow, function() {
        close_manager(aWindow, function() {
          open_manager(null, function(aWindow) {
            gCategoryUtilities = new CategoryUtilities(aWindow);
            is(gCategoryUtilities.selectedCategory, "discover", "Should show the discovery pane by default");

            close_manager(aWindow, finish);
          });
        });
      });
    }, aWindow);
  });
}
