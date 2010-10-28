/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests that history navigation works for the add-ons manager. Only used if
 * in-content UI is supported for this application.
 */

function test() {
  // XXX
  ok(true, "Test temporarily disabled due to timeouts\n");
  return;

  if (!gUseInContentUI)
    return;

  waitForExplicitFinish();

  var gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "test1@tests.mozilla.org",
    name: "Test add-on 1",
    description: "foo"
  },
  {
    id: "test2@tests.mozilla.org",
    name: "Test add-on 2",
    description: "bar"
  },
  {
    id: "test3@tests.mozilla.org",
    name: "Test add-on 3",
    type: "theme",
    description: "bar"
  }]);

  run_next_test();
}

function end_test() {
  finish();
}

function is_in_list(aManager, view, canGoBack, canGoForward) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, view, "Should be on the right category");
  is(doc.getElementById("view-port").selectedPanel.id, "list-view", "Should be on the right view");
  is(gBrowser.canGoBack, canGoBack, "canGoBack should be correct");
  is(!doc.getElementById("back-btn").disabled, canGoBack, "Back button should have the right state");
  is(gBrowser.canGoForward, canGoForward, "canGoForward should be correct");
  is(!doc.getElementById("forward-btn").disabled, canGoForward, "Forward button should have the right state");
}

function is_in_search(aManager, query, canGoBack, canGoForward) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, "addons://search/", "Should be on the right category");
  is(doc.getElementById("view-port").selectedPanel.id, "search-view", "Should be on the right view");
  is(doc.getElementById("header-search").value, query, "Should have used the right query");
  is(gBrowser.canGoBack, canGoBack, "canGoBack should be correct");
  is(!doc.getElementById("back-btn").disabled, canGoBack, "Back button should have the right state");
  is(gBrowser.canGoForward, canGoForward, "canGoForward should be correct");
  is(!doc.getElementById("forward-btn").disabled, canGoForward, "Forward button should have the right state");
}

function is_in_detail(aManager, view, canGoBack, canGoForward) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, view, "Should be on the right category");
  is(doc.getElementById("view-port").selectedPanel.id, "detail-view", "Should be on the right view");
  is(gBrowser.canGoBack, canGoBack, "canGoBack should be correct");
  is(!doc.getElementById("back-btn").disabled, canGoBack, "Back button should have the right state");
  is(gBrowser.canGoForward, canGoForward, "canGoForward should be correct");
  is(!doc.getElementById("forward-btn").disabled, canGoForward, "Forward button should have the right state");
}

// Tests simple forward and back navigation and that the right heading and
// category is selected
add_test(function() {
  open_manager("addons://list/extension", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugins"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_list(aManager, "addons://list/plugin", true, false);

      gBrowser.goBack();

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_list(aManager, "addons://list/extension", false, true);

        gBrowser.goForward();

        wait_for_view_load(aManager, function(aManager) {
          info("Part 4");
          is_in_list(aManager, "addons://list/plugin", true, false);

          gBrowser.goBack();

          wait_for_view_load(aManager, function(aManager) {
            info("Part 5");
            is_in_list(aManager, "addons://list/extension", false, true);

            EventUtils.synthesizeMouseAtCenter(get_addon_element(aManager, "test1@tests.mozilla.org"),
                                               { clickCount: 2 }, aManager);

            wait_for_view_load(aManager, function(aManager) {
              info("Part 6");
              is_in_detail(aManager, "addons://list/extension", true, false);

              gBrowser.goBack();

              wait_for_view_load(aManager, function(aManager) {
                info("Part 7");
                is_in_list(aManager, "addons://list/extension", false, true);

                close_manager(aManager, run_next_test);
              });
            });
          });
        });
      });
    });
  });
});

// Tests that browsing to the add-ons manager from a website and going back works
add_test(function() {
  info("Part 1");
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI("http://example.com/");
  gBrowser.addEventListener("pageshow", function(event) {
    if (event.target.location != "http://example.com/")
      return;
    gBrowser.removeEventListener("pageshow", arguments.callee, false);

    //Must let the load complete for it to go into the session history
    executeSoon(function() {
      info("Part 2");
      ok(!gBrowser.canGoBack, "Should not be able to go back");
      ok(!gBrowser.canGoForward, "Should not be able to go forward");

      gBrowser.loadURI("about:addons");
      gBrowser.addEventListener("pageshow", function(event) {
        if (event.target.location != "about:addons")
          return;
        gBrowser.removeEventListener("pageshow", arguments.callee, true);

        wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
          info("Part 3");
          is_in_list(aManager, "addons://list/extension", true, false);

          gBrowser.goBack();
          gBrowser.addEventListener("pageshow", function() {
            gBrowser.removeEventListener("pageshow", arguments.callee, false);
            info("Part 4");
            is(gBrowser.currentURI.spec, "http://example.com/", "Should be showing the webpage");
            ok(!gBrowser.canGoBack, "Should not be able to go back");
            ok(gBrowser.canGoForward, "Should be able to go forward");

            gBrowser.goForward();
            gBrowser.addEventListener("pageshow", function() {
              gBrowser.removeEventListener("pageshow", arguments.callee, false);
              wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
                info("Part 5");
                is_in_list(aManager, "addons://list/extension", true, false);

                close_manager(aManager, run_next_test);
              });
            }, false);
          }, false);
        });
      }, true);
    });
  }, false);
});

// Tests that opening a custom first view only stores a single history entry
add_test(function() {
  open_manager("addons://list/plugin", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/plugin", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-extensions"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_list(aManager, "addons://list/extension", true, false);

      gBrowser.goBack();

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_list(aManager, "addons://list/plugin", false, true);

        close_manager(aManager, run_next_test);
      });
    });
  });
});


// Tests that opening a view while the manager is already open adds a new
// history entry
add_test(function() {
  open_manager("addons://list/extension", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    aManager.loadView("addons://list/plugin");

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_list(aManager, "addons://list/plugin", true, false);

      gBrowser.goBack();

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_list(aManager, "addons://list/extension", false, true);

        gBrowser.goForward();

        wait_for_view_load(aManager, function(aManager) {
          info("Part 4");
          is_in_list(aManager, "addons://list/plugin", true, false);

          close_manager(aManager, run_next_test);
        });
      });
    });
  });
});

// Tests than navigating to a website and then going back returns to the
// previous view
add_test(function() {
  open_manager("addons://list/plugin", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/plugin", false, false);

    gBrowser.loadURI("http://example.com/");
    gBrowser.addEventListener("pageshow", function(event) {
      if (event.target.location != "http://example.com/")
        return;
      gBrowser.removeEventListener("pageshow", arguments.callee, false);
      info("Part 2");

      executeSoon(function() {
        ok(gBrowser.canGoBack, "Should be able to go back");
        ok(!gBrowser.canGoForward, "Should not be able to go forward");

        gBrowser.goBack();

        gBrowser.addEventListener("pageshow", function(event) {
          if (event.target.location != "about:addons")
            return;
          gBrowser.removeEventListener("pageshow", arguments.callee, false);

          wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
            info("Part 3");
            is_in_list(aManager, "addons://list/plugin", false, true);

            gBrowser.goForward();
            gBrowser.addEventListener("pageshow", function(event) {
              if (event.target.location != "http://example.com/")
                return;
              gBrowser.removeEventListener("pageshow", arguments.callee, false);
              info("Part 4");

              executeSoon(function() {
                ok(gBrowser.canGoBack, "Should be able to go back");
                ok(!gBrowser.canGoForward, "Should not be able to go forward");

                gBrowser.goBack();

                gBrowser.addEventListener("pageshow", function(event) {
                  if (event.target.location != "about:addons")
                    return;
                  gBrowser.removeEventListener("pageshow", arguments.callee, false);
                  wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
                    info("Part 5");
                    is_in_list(aManager, "addons://list/plugin", false, true);

                    close_manager(aManager, run_next_test);
                  });
                }, false);
              });
            }, false);
          });
        }, false);
      });
    }, false);
  });
});

// Tests that going back to search results works
add_test(function() {
  open_manager("addons://list/extension", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    var search = aManager.document.getElementById("header-search");
    search.focus();
    search.value = "bar";
    EventUtils.synthesizeKey("VK_RETURN", {});

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_search(aManager, "bar", true, false);
      check_all_in_list(aManager, ["test2@tests.mozilla.org", "test3@tests.mozilla.org"]);

      EventUtils.synthesizeMouseAtCenter(get_addon_element(aManager, "test2@tests.mozilla.org"),
                                         { clickCount: 2 }, aManager);

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_detail(aManager, "addons://search/", true, false);

        gBrowser.goBack();
        wait_for_view_load(aManager, function(aManager) {
          info("Part 4");
          is_in_search(aManager, "bar", true, true);
          check_all_in_list(aManager, ["test2@tests.mozilla.org", "test3@tests.mozilla.org"]);

          gBrowser.goForward();
          wait_for_view_load(aManager, function(aManager) {
            info("Part 5");
            is_in_detail(aManager, "addons://search/", true, false);

            close_manager(aManager, run_next_test);
          });
        });
      });
    });
  });
});

// Tests that going back to a detail view loaded from a search result works
add_test(function() {
  open_manager(null, function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    var search = aManager.document.getElementById("header-search");
    search.focus();
    search.value = "bar";
    EventUtils.synthesizeKey("VK_RETURN", {});

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_search(aManager, "bar", true, false);
      check_all_in_list(aManager, ["test2@tests.mozilla.org", "test3@tests.mozilla.org"]);

      EventUtils.synthesizeMouseAtCenter(get_addon_element(aManager, "test2@tests.mozilla.org"),
                                         { clickCount: 2 }, aManager);

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_detail(aManager, "addons://search/", true, false);

        gBrowser.loadURI("http://example.com/");
        gBrowser.addEventListener("pageshow", function(event) {
          if (event.target.location != "http://example.com/")
            return;
          gBrowser.removeEventListener("pageshow", arguments.callee, false);

          info("Part 4");
          executeSoon(function() {
            ok(gBrowser.canGoBack, "Should be able to go back");
            ok(!gBrowser.canGoForward, "Should not be able to go forward");

            gBrowser.goBack();
            gBrowser.addEventListener("pageshow", function(event) {
                if (event.target.location != "about:addons")
                return;
              gBrowser.removeEventListener("pageshow", arguments.callee, false);

              wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
                info("Part 5");
                is_in_detail(aManager, "addons://search/", true, true);

                gBrowser.goBack();
                wait_for_view_load(aManager, function(aManager) {
                  info("Part 6");
                  is_in_search(aManager, "bar", true, true);
                  check_all_in_list(aManager, ["test2@tests.mozilla.org", "test3@tests.mozilla.org"]);

                  close_manager(aManager, run_next_test);
                });
              });
            }, false);
          });
        }, false);
      });
    });
  });
});

// Tests that refreshing a list view does not affect the history
add_test(function() {
  open_manager(null, function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugins"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_list(aManager, "addons://list/plugin", true, false);

      gBrowser.reload();
      gBrowser.addEventListener("pageshow", function(event) {
          if (event.target.location != "about:addons")
          return;
        gBrowser.removeEventListener("pageshow", arguments.callee, false);

        wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
          info("Part 3");
          is_in_list(aManager, "addons://list/plugin", true, false);

          gBrowser.goBack();
          wait_for_view_load(aManager, function(aManager) {
            info("Part 4");
            is_in_list(aManager, "addons://list/extension", false, true);

            close_manager(aManager, run_next_test);
          });
        });
      }, false);
    });
  });
});

// Tests that refreshing a detail view does not affect the history
add_test(function() {
  open_manager(null, function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    EventUtils.synthesizeMouseAtCenter(get_addon_element(aManager, "test1@tests.mozilla.org"),
                                       { clickCount: 2 }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_detail(aManager, "addons://list/extension", true, false);

      gBrowser.reload();
      gBrowser.addEventListener("pageshow", function(event) {
          if (event.target.location != "about:addons")
          return;
        gBrowser.removeEventListener("pageshow", arguments.callee, false);

        wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
          info("Part 3");
          is_in_detail(aManager, "addons://list/extension", true, false);

          gBrowser.goBack();
          wait_for_view_load(aManager, function(aManager) {
            info("Part 4");
            is_in_list(aManager, "addons://list/extension", false, true);

            close_manager(aManager, run_next_test);
          });
        });
      }, false);
    });
  });
});

// Tests that removing an extension from the detail view goes back and doesn't
// allow you to go forward again.
add_test(function() {
  open_manager(null, function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    EventUtils.synthesizeMouseAtCenter(get_addon_element(aManager, "test1@tests.mozilla.org"),
                                       { clickCount: 2 }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_detail(aManager, "addons://list/extension", true, false);

      EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("detail-uninstall-btn"),
                                         { }, aManager);

      wait_for_view_load(aManager, function() {
        // TODO until bug 590661 is fixed the back button will be enabled
        is_in_list(aManager, "addons://list/extension", true, false);

        close_manager(aManager, run_next_test);
      });
    });
  });
});
