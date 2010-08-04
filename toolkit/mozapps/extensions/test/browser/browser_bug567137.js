/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that the selected category is persisted across loads of the manager

function test() {
  waitForExplicitFinish();

  open_manager(null, function(aWindow) {
    let utils = new CategoryUtilities(aWindow);

    // Open the plugins category
    utils.openType("plugin", function() {

      // Re-open the manager
      close_manager(aWindow, function() {
        open_manager(null, function(aWindow) {
          utils = new CategoryUtilities(aWindow);

          is(utils.selectedCategory, "plugin", "Should have shown the plugins category");

          // Open the extensions category
          utils.openType("extension", function() {

            // Re-open the manager
            close_manager(aWindow, function() {
              open_manager(null, function(aWindow) {
                utils = new CategoryUtilities(aWindow);

                is(utils.selectedCategory, "extension", "Should have shown the extensions category");
                close_manager(aWindow, finish);
              });
            });
          });
        });
      });
    });
  });
}
