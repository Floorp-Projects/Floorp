/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 608316 - Test that opening the manager to an add-on that doesn't exist
// just loads the default view

var gCategoryUtilities;

function test() {
  waitForExplicitFinish();

  run_next_test();
}

function end_test() {
  finish();
}

add_test(function() {
  open_manager("addons://detail/foo", function(aManager) {
    gCategoryUtilities = new CategoryUtilities(aManager);
    is(gCategoryUtilities.selectedCategory, "discover", "Should fall back to the discovery pane");

    close_manager(aManager, run_next_test);
  });
});

// Also test that opening directly to an add-on that does exist doesn't break
// and selects the right category
add_test(function() {
  new MockProvider().createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "addon 1",
    version: "1.0"
  }]);

  open_manager("addons://detail/addon1@tests.mozilla.org", function(aManager) {
    gCategoryUtilities = new CategoryUtilities(aManager);
    is(gCategoryUtilities.selectedCategory, "extension", "Should have selected the right category");

    close_manager(aManager, run_next_test);
  });
});
