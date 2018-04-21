/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that the selected category is persisted across loads of the manager

async function test() {
  waitForExplicitFinish();

  let aWindow = await open_manager(null);
  let utils = new CategoryUtilities(aWindow);

  // Open the plugins category
  utils.openType("plugin", async function() {

    // Re-open the manager
    await close_manager(aWindow);
    aWindow = await open_manager(null);
    utils = new CategoryUtilities(aWindow);

    is(utils.selectedCategory, "plugin", "Should have shown the plugins category");

    // Open the extensions category
    utils.openType("extension", async function() {

      // Re-open the manager
      await close_manager(aWindow);
      aWindow = await open_manager(null);
      utils = new CategoryUtilities(aWindow);

      is(utils.selectedCategory, "extension", "Should have shown the extensions category");
      close_manager(aWindow, finish);
    });
  });
}
