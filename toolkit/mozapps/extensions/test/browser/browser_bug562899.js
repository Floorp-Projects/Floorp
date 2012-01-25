/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Simulates quickly switching between different list views to verify that only
// the last selected is displayed

let tempScope = {};
Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", tempScope);
let LightweightThemeManager = tempScope.LightweightThemeManager;

const xpi = "browser/toolkit/mozapps/extensions/test/browser/browser_installssl.xpi";

var gManagerWindow;
var gCategoryUtilities;

function test() {
  waitForExplicitFinish();

  // Add a lightweight theme so at least one theme exists
  LightweightThemeManager.currentTheme = {
    id: "test",
    name: "Test lightweight theme",
    headerURL: "http://example.com/header.png"
  };

  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    LightweightThemeManager.forgetUsedTheme("test");
    finish();
  });
}

// Tests that loading a second view before the first has not finished loading
// does not merge the results
add_test(function() {
  var themeCount = null;
  var pluginCount = null;
  var themeItem = gCategoryUtilities.get("theme");
  var pluginItem = gCategoryUtilities.get("plugin");
  var list = gManagerWindow.document.getElementById("addon-list");

  gCategoryUtilities.open(themeItem, function() {
    themeCount = list.childNodes.length;
    ok(themeCount > 0, "Test is useless if there are no themes");

    gCategoryUtilities.open(pluginItem, function() {
      pluginCount = list.childNodes.length;
      ok(pluginCount > 0, "Test is useless if there are no plugins");

      gCategoryUtilities.open(themeItem);

      gCategoryUtilities.open(pluginItem, function() {
        is(list.childNodes.length, pluginCount, "Should only see the plugins");

        var item = list.firstChild;
        while (item) {
          is(item.getAttribute("type"), "plugin", "All items should be plugins");
          item = item.nextSibling;
        }

        // Tests that switching to, from, to the same pane in quick succession
        // still only shows the right number of results

        gCategoryUtilities.open(themeItem);
        gCategoryUtilities.open(pluginItem);
        gCategoryUtilities.open(themeItem, function() {
          is(list.childNodes.length, themeCount, "Should only see the theme");

          var item = list.firstChild;
          while (item) {
            is(item.getAttribute("type"), "theme", "All items should be theme");
            item = item.nextSibling;
          }

          run_next_test();
        });
      });
    });
  });
});
