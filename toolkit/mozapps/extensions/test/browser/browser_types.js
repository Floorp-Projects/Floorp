/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that registering new types works

var gManagerWindow;
var gCategoryUtilities;

var gProvider = {
};

var gTypes = [
  new AddonManagerPrivate.AddonType("type1", null, "Type 1",
                                    AddonManager.VIEW_TYPE_LIST, 4500),
  new AddonManagerPrivate.AddonType("missing1", null, "Missing 1"),
  new AddonManagerPrivate.AddonType("type2", null, "Type 1",
                                    AddonManager.VIEW_TYPE_LIST, 5100,
                                    AddonManager.TYPE_UI_HIDE_EMPTY),
  {
    id: "type3",
    name: "Type 3",
    uiPriority: 5200,
    viewType: AddonManager.VIEW_TYPE_LIST
  }
];

function go_back(aManager) {
  if (gUseInContentUI) {
    gBrowser.goBack();
  } else {
    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("back-btn"),
                                       { }, aManager);
  }
}

function go_forward(aManager) {
  if (gUseInContentUI) {
    gBrowser.goForward();
  } else {
    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("forward-btn"),
                                       { }, aManager);
  }
}

function check_state(aManager, canGoBack, canGoForward) {
  var doc = aManager.document;

  if (gUseInContentUI) {
    is(gBrowser.canGoBack, canGoBack, "canGoBack should be correct");
    is(gBrowser.canGoForward, canGoForward, "canGoForward should be correct");
  }

  if (!is_hidden(doc.getElementById("back-btn"))) {
    is(!doc.getElementById("back-btn").disabled, canGoBack, "Back button should have the right state");
    is(!doc.getElementById("forward-btn").disabled, canGoForward, "Forward button should have the right state");
  }
}

function test() {
  waitForExplicitFinish();

  run_next_test();
}

function end_test() {
  finish();
}

// Add a new type, open the manager and make sure it is in the right place
add_test(function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.get("type2"), "Type 2 should be present");
    ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

    is(gCategoryUtilities.get("type1").previousSibling.getAttribute("value"),
       "addons://list/extension", "Type 1 should be in the right place");
    is(gCategoryUtilities.get("type2").previousSibling.getAttribute("value"),
       "addons://list/theme", "Type 2 should be in the right place");

    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");
    ok(!gCategoryUtilities.isTypeVisible("type2"), "Type 2 should be hidden");

    run_next_test();
  });
});

// Select the type, close the manager and remove it then open the manager and
// check we're back to the default view
add_test(function() {
  gCategoryUtilities.openType("type1", function() {
    close_manager(gManagerWindow, function() {
      AddonManagerPrivate.unregisterProvider(gProvider);

      open_manager(null, function(aWindow) {
        gManagerWindow = aWindow;
        gCategoryUtilities = new CategoryUtilities(gManagerWindow);

        ok(!gCategoryUtilities.get("type1", true), "Type 1 should be absent");
        ok(!gCategoryUtilities.get("type2", true), "Type 2 should be absent");
        ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

        is(gCategoryUtilities.selectedCategory, "discover", "Should be back to the default view");

        close_manager(gManagerWindow, run_next_test);
      });
    });
  });
});

// Add a type while the manager is still open and check it appears
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    ok(!gCategoryUtilities.get("type1", true), "Type 1 should be absent");
    ok(!gCategoryUtilities.get("type2", true), "Type 2 should be absent");
    ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

    AddonManagerPrivate.registerProvider(gProvider, gTypes);

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.get("type2"), "Type 2 should be present");
    ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

    is(gCategoryUtilities.get("type1").previousSibling.getAttribute("value"),
       "addons://list/extension", "Type 1 should be in the right place");
    is(gCategoryUtilities.get("type2").previousSibling.getAttribute("value"),
       "addons://list/theme", "Type 2 should be in the right place");

    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");
    ok(!gCategoryUtilities.isTypeVisible("type2"), "Type 2 should be hidden");

    run_next_test();
  });
});

// Remove the type while it is beng viewed and check it is replaced with the
// default view
add_test(function() {
  gCategoryUtilities.openType("type1", function() {
    gCategoryUtilities.openType("plugin", function() {
      go_back(gManagerWindow);
      wait_for_view_load(gManagerWindow, function() {
        is(gCategoryUtilities.selectedCategory, "type1", "Should be showing the custom view");
        check_state(gManagerWindow, true, true);

        AddonManagerPrivate.unregisterProvider(gProvider);

        ok(!gCategoryUtilities.get("type1", true), "Type 1 should be absent");
        ok(!gCategoryUtilities.get("type2", true), "Type 2 should be absent");
        ok(!gCategoryUtilities.get("missing1", true), "Missing 1 should be absent");

        is(gCategoryUtilities.selectedCategory, "discover", "Should be back to the default view");
        check_state(gManagerWindow, true, true);

        go_back(gManagerWindow);
        wait_for_view_load(gManagerWindow, function() {
          is(gCategoryUtilities.selectedCategory, "extension", "Should be showing the extension view");
          check_state(gManagerWindow, false, true);

          go_forward(gManagerWindow);
          wait_for_view_load(gManagerWindow, function() {
            is(gCategoryUtilities.selectedCategory, "discover", "Should be back to the default view");
            check_state(gManagerWindow, true, true);

            go_forward(gManagerWindow);
            wait_for_view_load(gManagerWindow, function() {
              is(gCategoryUtilities.selectedCategory, "plugin", "Should be back to the plugins view");
              check_state(gManagerWindow, true, false);

              go_back(gManagerWindow);
              wait_for_view_load(gManagerWindow, function() {
                is(gCategoryUtilities.selectedCategory, "discover", "Should be back to the default view");
                check_state(gManagerWindow, true, true);

                close_manager(gManagerWindow, run_next_test);
              });
            });
          });
        });
      });
    });
  });
});

// Test that when going back to a now missing category we skip it
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    AddonManagerPrivate.registerProvider(gProvider, gTypes);

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

    gCategoryUtilities.openType("type1", function() {
      gCategoryUtilities.openType("plugin", function() {
        AddonManagerPrivate.unregisterProvider(gProvider);

        ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

        go_back(gManagerWindow);

        wait_for_view_load(gManagerWindow, function() {
          is(gCategoryUtilities.selectedCategory, "extension", "Should be back to the first view");
          check_state(gManagerWindow, false, true);

          close_manager(gManagerWindow, run_next_test);
        });
      });
    });
  });
});

// Test that when going forward to a now missing category we skip it
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    AddonManagerPrivate.registerProvider(gProvider, gTypes);

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

    gCategoryUtilities.openType("type1", function() {
      gCategoryUtilities.openType("plugin", function() {
        go_back(gManagerWindow);
        wait_for_view_load(gManagerWindow, function() {
          go_back(gManagerWindow);
          wait_for_view_load(gManagerWindow, function() {
            is(gCategoryUtilities.selectedCategory, "extension", "Should be back to the extension view");

            AddonManagerPrivate.unregisterProvider(gProvider);

            ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

            go_forward(gManagerWindow);

            wait_for_view_load(gManagerWindow, function() {
              is(gCategoryUtilities.selectedCategory, "plugin", "Should be back to the plugin view");
              check_state(gManagerWindow, true, false);

              close_manager(gManagerWindow, run_next_test);
            });
          });
        });
      });
    });
  });
});

// Test that when going back to a now missing category and we can't go back any
// any further then we just display the default view
add_test(function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  open_manager("addons://list/type1", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    is(gCategoryUtilities.selectedCategory, "type1", "Should be at the custom view");

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

    gCategoryUtilities.openType("extension", function() {
      AddonManagerPrivate.unregisterProvider(gProvider);

      ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

      go_back(gManagerWindow);

      wait_for_view_load(gManagerWindow, function() {
        is(gCategoryUtilities.selectedCategory, "discover", "Should be at the default view");
        check_state(gManagerWindow, false, true);

        close_manager(gManagerWindow, run_next_test);
      });
    });
  });
});

// Test that when going forward to a now missing category and we can't go
// forward any further then we just display the default view
add_test(function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

    gCategoryUtilities.openType("type1", function() {
      go_back(gManagerWindow);

      wait_for_view_load(gManagerWindow, function() {
        is(gCategoryUtilities.selectedCategory, "extension", "Should be at the extension view");

        AddonManagerPrivate.unregisterProvider(gProvider);

        ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

        go_forward(gManagerWindow);

        wait_for_view_load(gManagerWindow, function() {
          is(gCategoryUtilities.selectedCategory, "discover", "Should be at the default view");
          check_state(gManagerWindow, true, false);

          close_manager(gManagerWindow, run_next_test);
        });
      });
    });
  });
});

// Test that when going back we skip multiple missing categories
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    AddonManagerPrivate.registerProvider(gProvider, gTypes);

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

    gCategoryUtilities.openType("type1", function() {
      gCategoryUtilities.openType("type3", function() {
        gCategoryUtilities.openType("plugin", function() {
          AddonManagerPrivate.unregisterProvider(gProvider);

          ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

          go_back(gManagerWindow);

          wait_for_view_load(gManagerWindow, function() {
            is(gCategoryUtilities.selectedCategory, "extension", "Should be back to the first view");
            check_state(gManagerWindow, false, true);

            close_manager(gManagerWindow, run_next_test);
          });
        });
      });
    });
  });
});

// Test that when going forward we skip multiple missing categories
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    AddonManagerPrivate.registerProvider(gProvider, gTypes);

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

    gCategoryUtilities.openType("type1", function() {
      gCategoryUtilities.openType("type3", function() {
        gCategoryUtilities.openType("plugin", function() {
          go_back(gManagerWindow);
          wait_for_view_load(gManagerWindow, function() {
            go_back(gManagerWindow);
            wait_for_view_load(gManagerWindow, function() {
              go_back(gManagerWindow);
              wait_for_view_load(gManagerWindow, function() {
                is(gCategoryUtilities.selectedCategory, "extension", "Should be back to the extension view");

                AddonManagerPrivate.unregisterProvider(gProvider);

                ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

                go_forward(gManagerWindow);

                wait_for_view_load(gManagerWindow, function() {
                  is(gCategoryUtilities.selectedCategory, "plugin", "Should be back to the plugin view");
                  check_state(gManagerWindow, true, false);

                  close_manager(gManagerWindow, run_next_test);
                });
              });
            });
          });
        });
      });
    });
  });
});

// Test that when going back we skip all missing categories and when we can't go
// back any any further then we just display the default view
add_test(function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  open_manager("addons://list/type1", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    is(gCategoryUtilities.selectedCategory, "type1", "Should be at the custom view");

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

    gCategoryUtilities.openType("type3", function() {
      gCategoryUtilities.openType("extension", function() {
        AddonManagerPrivate.unregisterProvider(gProvider);

        ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

        go_back(gManagerWindow);

        wait_for_view_load(gManagerWindow, function() {
          is(gCategoryUtilities.selectedCategory, "discover", "Should be at the default view");
          check_state(gManagerWindow, false, true);

          close_manager(gManagerWindow, run_next_test);
        });
      });
    });
  });
});

// Test that when going forward we skip all missing categories and when we can't
// go back any any further then we just display the default view
add_test(function() {
  AddonManagerPrivate.registerProvider(gProvider, gTypes);

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    ok(gCategoryUtilities.get("type1"), "Type 1 should be present");
    ok(gCategoryUtilities.isTypeVisible("type1"), "Type 1 should be visible");

    gCategoryUtilities.openType("type1", function() {
      gCategoryUtilities.openType("type3", function() {
        go_back(gManagerWindow);

        wait_for_view_load(gManagerWindow, function() {
          go_back(gManagerWindow);

          wait_for_view_load(gManagerWindow, function() {
            is(gCategoryUtilities.selectedCategory, "extension", "Should be at the extension view");

            AddonManagerPrivate.unregisterProvider(gProvider);

            ok(!gCategoryUtilities.get("type1", true), "Type 1 should not be present");

            go_forward(gManagerWindow);

            wait_for_view_load(gManagerWindow, function() {
              is(gCategoryUtilities.selectedCategory, "discover", "Should be at the default view");
              check_state(gManagerWindow, true, false);

              close_manager(gManagerWindow, run_next_test);
            });
          });
        });
      });
    });
  });
});
