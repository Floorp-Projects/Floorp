/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests that history navigation works for the add-ons manager.
 */

const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery.html";
const SECOND_URL = "https://example.com/" + RELATIVE_DIR + "releaseNotes.xhtml";

var gLoadCompleteCallback = null;

var gProgressListener = {
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    // Only care about the network stop status events
    if (!(aStateFlags & (Ci.nsIWebProgressListener.STATE_IS_NETWORK)) ||
        !(aStateFlags & (Ci.nsIWebProgressListener.STATE_STOP)))
      return;

    if (gLoadCompleteCallback)
      executeSoon(gLoadCompleteCallback);
    gLoadCompleteCallback = null;
  },

  onLocationChange: function() { },
  onSecurityChange: function() { },
  onProgressChange: function() { },
  onStatusChange: function() { },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),
};

function waitForLoad(aManager, aCallback) {
  var browser = aManager.document.getElementById("discover-browser");
  browser.addProgressListener(gProgressListener);

  gLoadCompleteCallback = function() {
    browser.removeProgressListener(gProgressListener);
    aCallback();
  };
}

function clickLink(aManager, aId, aCallback) {
  waitForLoad(aManager, aCallback);

  var browser = aManager.document.getElementById("discover-browser");

  var link = browser.contentDocument.getElementById(aId);
  EventUtils.sendMouseEvent({type: "click"}, link);
}

function test() {
  requestLongerTimeout(2);

  waitForExplicitFinish();

  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);

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

function go_back(aManager) {
  if (gUseInContentUI) {
    gBrowser.goBack();
  } else {
    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("back-btn"),
                                       { }, aManager);
  }
}

function go_back_backspace(aManager) {
    EventUtils.synthesizeKey("VK_BACK_SPACE",{});
}

function go_forward_backspace(aManager) {
    EventUtils.synthesizeKey("VK_BACK_SPACE",{shiftKey: true});
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

function is_in_list(aManager, view, canGoBack, canGoForward) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, view, "Should be on the right category");
  is(doc.getElementById("view-port").selectedPanel.id, "list-view", "Should be on the right view");

  check_state(aManager, canGoBack, canGoForward);
}

function is_in_search(aManager, query, canGoBack, canGoForward) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, "addons://search/", "Should be on the right category");
  is(doc.getElementById("view-port").selectedPanel.id, "search-view", "Should be on the right view");
  is(doc.getElementById("header-search").value, query, "Should have used the right query");

  check_state(aManager, canGoBack, canGoForward);
}

function is_in_detail(aManager, view, canGoBack, canGoForward) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, view, "Should be on the right category");
  is(doc.getElementById("view-port").selectedPanel.id, "detail-view", "Should be on the right view");

  check_state(aManager, canGoBack, canGoForward);
}

function is_in_discovery(aManager, url, canGoBack, canGoForward) {
  var browser = aManager.document.getElementById("discover-browser");

  is(aManager.document.getElementById("discover-view").selectedPanel, browser,
     "Browser should be visible");

  var spec = browser.currentURI.spec;
  var pos = spec.indexOf("#");
  if (pos != -1)
    spec = spec.substring(0, pos);

  is(spec, url, "Should have loaded the right url");

  check_state(aManager, canGoBack, canGoForward);
}

function double_click_addon_element(aManager, aId) {
  var addon = get_addon_element(aManager, aId);
  addon.parentNode.ensureElementIsVisible(addon);
  EventUtils.synthesizeMouseAtCenter(addon, { clickCount: 1 }, aManager);
  EventUtils.synthesizeMouseAtCenter(addon, { clickCount: 2 }, aManager);
}

// Tests simple forward and back navigation and that the right heading and
// category is selected
add_test(function() {
  open_manager("addons://list/extension", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_list(aManager, "addons://list/plugin", true, false);

      go_back(aManager);

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_list(aManager, "addons://list/extension", false, true);

        go_forward(aManager);

        wait_for_view_load(aManager, function(aManager) {
          info("Part 4");
          is_in_list(aManager, "addons://list/plugin", true, false);

          go_back(aManager);

          wait_for_view_load(aManager, function(aManager) {
            info("Part 5");
            is_in_list(aManager, "addons://list/extension", false, true);

            double_click_addon_element(aManager, "test1@tests.mozilla.org");

            wait_for_view_load(aManager, function(aManager) {
              info("Part 6");
              is_in_detail(aManager, "addons://list/extension", true, false);

              go_back(aManager);

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
// Only relevant for in-content UI
add_test(function() {
  if (!gUseInContentUI) {
    run_next_test();
    return;
  }

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

          go_back(aManager);
          gBrowser.addEventListener("pageshow", function() {
            gBrowser.removeEventListener("pageshow", arguments.callee, false);
            info("Part 4");
            is(gBrowser.currentURI.spec, "http://example.com/", "Should be showing the webpage");
            ok(!gBrowser.canGoBack, "Should not be able to go back");
            ok(gBrowser.canGoForward, "Should be able to go forward");

            go_forward(aManager);
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

// Tests simple forward and back navigation and that the right heading and
// category is selected -- Keyboard navigation [Bug 565359]
// Only add the test if the backspace key navigates back and addon-manager
// loaded in a tab
add_test(function() {

  if (!gUseInContentUI || (Services.prefs.getIntPref("browser.backspace_action") != 0)) {
    run_next_test();
    return;
  }

  open_manager("addons://list/extension", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_list(aManager, "addons://list/plugin", true, false);

      go_back_backspace(aManager);

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_list(aManager, "addons://list/extension", false, true);

        go_forward_backspace(aManager);

        wait_for_view_load(aManager, function(aManager) {
          info("Part 4");
          is_in_list(aManager, "addons://list/plugin", true, false);

          go_back_backspace(aManager);

          wait_for_view_load(aManager, function(aManager) {
            info("Part 5");
            is_in_list(aManager, "addons://list/extension", false, true);

            double_click_addon_element(aManager, "test1@tests.mozilla.org");

            wait_for_view_load(aManager, function(aManager) {
              info("Part 6");
              is_in_detail(aManager, "addons://list/extension", true, false);

              go_back_backspace(aManager);

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


// Tests that opening a custom first view only stores a single history entry
add_test(function() {
  open_manager("addons://list/plugin", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/plugin", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-extension"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_list(aManager, "addons://list/extension", true, false);

      go_back(aManager);

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

      go_back(aManager);

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_list(aManager, "addons://list/extension", false, true);

        go_forward(aManager);

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
// Only relevant for in-content UI
add_test(function() {
  if (!gUseInContentUI) {
    run_next_test();
    return;
  }

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

        go_back(aManager);

        gBrowser.addEventListener("pageshow", function(event) {
          if (event.target.location != "about:addons")
            return;
          gBrowser.removeEventListener("pageshow", arguments.callee, false);

          wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
            info("Part 3");
            is_in_list(aManager, "addons://list/plugin", false, true);

            go_forward(aManager);
            gBrowser.addEventListener("pageshow", function(event) {
              if (event.target.location != "http://example.com/")
                return;
              gBrowser.removeEventListener("pageshow", arguments.callee, false);
              info("Part 4");

              executeSoon(function() {
                ok(gBrowser.canGoBack, "Should be able to go back");
                ok(!gBrowser.canGoForward, "Should not be able to go forward");

                go_back(aManager);

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
  // Before we open the add-ons manager, we should make sure that no filter
  // has been set. If one is set, we remove it.
  // This is for the check below, from bug 611459.
  let RDF = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
  let store = RDF.GetDataSource("rdf:local-store");
  let filterResource = RDF.GetResource("about:addons#search-filter-radiogroup");
  let filterProperty = RDF.GetResource("value");
  let filterTarget = store.GetTarget(filterResource, filterProperty, true);

  if (filterTarget) {
    is(filterTarget instanceof Ci.nsIRDFLiteral, true,
       "Filter should be a value");
    store.Unassert(filterResource, filterProperty, filterTarget);
  }

  open_manager("addons://list/extension", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    var search = aManager.document.getElementById("header-search");
    search.focus();
    search.value = "bar";
    EventUtils.synthesizeKey("VK_RETURN", {}, aManager);

    wait_for_view_load(aManager, function(aManager) {
      // Remote search is meant to be checked by default (bug 611459), so we
      // confirm that and then switch to a local search.
      var localFilter = aManager.document.getElementById("search-filter-local");
      var remoteFilter = aManager.document.getElementById("search-filter-remote");

      is(remoteFilter.selected, true, "Remote filter should be set by default");

      var list = aManager.document.getElementById("search-list");
      list.ensureElementIsVisible(localFilter);
      EventUtils.synthesizeMouseAtCenter(localFilter, { }, aManager);

      is(localFilter.selected, true, "Should have changed to local filter");

      // Now we continue with the normal test.

      info("Part 2");
      is_in_search(aManager, "bar", true, false);
      check_all_in_list(aManager, ["test2@tests.mozilla.org", "test3@tests.mozilla.org"]);

      double_click_addon_element(aManager, "test2@tests.mozilla.org");

      wait_for_view_load(aManager, function(aManager) {
        info("Part 3");
        is_in_detail(aManager, "addons://search/", true, false);

        go_back(aManager);
        wait_for_view_load(aManager, function(aManager) {
          info("Part 4");
          is_in_search(aManager, "bar", true, true);
          check_all_in_list(aManager, ["test2@tests.mozilla.org", "test3@tests.mozilla.org"]);

          go_forward(aManager);
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

// Tests that going back from a webpage to a detail view loaded from a search
// result works
// Only relevant for in-content UI
add_test(function() {
  if (!gUseInContentUI) {
    run_next_test();
    return;
  }

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

      double_click_addon_element(aManager, "test2@tests.mozilla.org");

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

            go_back(aManager);
            gBrowser.addEventListener("pageshow", function(event) {
                if (event.target.location != "about:addons")
                return;
              gBrowser.removeEventListener("pageshow", arguments.callee, false);

              wait_for_view_load(gBrowser.contentWindow.wrappedJSObject, function(aManager) {
                info("Part 5");
                is_in_detail(aManager, "addons://search/", true, true);

                go_back(aManager);
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
// Only relevant for in-content UI
add_test(function() {
  if (!gUseInContentUI) {
    run_next_test();
    return;
  }

  open_manager("addons://list/extension", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

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

          go_back(aManager);
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
// Only relevant for in-content UI
add_test(function() {
  if (!gUseInContentUI) {
    run_next_test();
    return;
  }

  open_manager(null, function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    double_click_addon_element(aManager, "test1@tests.mozilla.org");

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

          go_back(aManager);
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
  open_manager("addons://list/extension", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/extension", false, false);

    double_click_addon_element(aManager, "test1@tests.mozilla.org");

    wait_for_view_load(aManager, function(aManager) {
      info("Part 2");
      is_in_detail(aManager, "addons://list/extension", true, false);

      EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("detail-uninstall-btn"),
                                         { }, aManager);

      wait_for_view_load(aManager, function() {
        if (gUseInContentUI) {
          // TODO until bug 590661 is fixed the back button will be enabled
          // when displaying in content
          is_in_list(aManager, "addons://list/extension", true, false);
        } else {
          is_in_list(aManager, "addons://list/extension", false, false);
        }

        close_manager(aManager, run_next_test);
      });
    });
  });
});

// Tests that the back and forward buttons only show up for windowed mode
add_test(function() {
  open_manager(null, function(aManager) {
    var doc = aManager.document;

    if (gUseInContentUI) {
      var btn = document.getElementById("back-button");
      if (!btn || is_hidden(btn)) {
        is_element_visible(doc.getElementById("back-btn"), "Back button should not be hidden");
        is_element_visible(doc.getElementById("forward-btn"), "Forward button should not be hidden");
      } else {
        is_element_hidden(doc.getElementById("back-btn"), "Back button should be hidden");
        is_element_hidden(doc.getElementById("forward-btn"), "Forward button should be hidden");
      }
    } else {
      is_element_visible(doc.getElementById("back-btn"), "Back button should not be hidden");
      is_element_visible(doc.getElementById("forward-btn"), "Forward button should not be hidden");
    }

    close_manager(aManager, run_next_test);
  });
});

// Tests that opening the manager opens the last view
add_test(function() {
  open_manager("addons://list/plugin", function(aManager) {
    info("Part 1");
    is_in_list(aManager, "addons://list/plugin", false, false);

    close_manager(aManager, function() {
      open_manager(null, function(aManager) {
        info("Part 2");
        is_in_list(aManager, "addons://list/plugin", false, false);

        close_manager(aManager, run_next_test);
      });
    });
  });
});

// Tests that navigating the discovery page works when that was the first view
add_test(function() {
  open_manager("addons://discover/", function(aManager) {
    info("1");
    is_in_discovery(aManager, MAIN_URL, false, false);

    clickLink(aManager, "link-good", function() {
      info("2");
      is_in_discovery(aManager, SECOND_URL, true, false);

      waitForLoad(aManager, function() {
        info("3");
        is_in_discovery(aManager, MAIN_URL, false, true);

        waitForLoad(aManager, function() {
          is_in_discovery(aManager, SECOND_URL, true, false);

          EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

          wait_for_view_load(aManager, function(aManager) {
            is_in_list(aManager, "addons://list/plugin", true, false);

            go_back(aManager);

            wait_for_view_load(aManager, function(aManager) {
              is_in_discovery(aManager, SECOND_URL, true, true);

              go_back(aManager);

              waitForLoad(aManager, function() {
                is_in_discovery(aManager, MAIN_URL, false, true);

                close_manager(aManager, run_next_test);
              });
            });
          });
        });

        go_forward(aManager);
      });

      go_back(aManager);
    });
  });
});

// Tests that navigating the discovery page works when that was the second view
add_test(function() {
  open_manager("addons://list/plugin", function(aManager) {
    is_in_list(aManager, "addons://list/plugin", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-discover"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      is_in_discovery(aManager, MAIN_URL, true, false);

      clickLink(aManager, "link-good", function() {
        is_in_discovery(aManager, SECOND_URL, true, false);

        waitForLoad(aManager, function() {
          is_in_discovery(aManager, MAIN_URL, true, true);

          waitForLoad(aManager, function() {
            is_in_discovery(aManager, SECOND_URL, true, false);

            EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

            wait_for_view_load(aManager, function(aManager) {
              is_in_list(aManager, "addons://list/plugin", true, false);

              go_back(aManager);

              wait_for_view_load(aManager, function(aManager) {
                is_in_discovery(aManager, SECOND_URL, true, true);

                go_back(aManager);

                waitForLoad(aManager, function() {
                  is_in_discovery(aManager, MAIN_URL, true, true);

                  go_back(aManager);

                  wait_for_view_load(aManager, function(aManager) {
                    is_in_list(aManager, "addons://list/plugin", false, true);

                    go_forward(aManager);

                    wait_for_view_load(aManager, function(aManager) {
                      is_in_discovery(aManager, MAIN_URL, true, true);

                      waitForLoad(aManager, function() {
                        is_in_discovery(aManager, SECOND_URL, true, true);

                        close_manager(aManager, run_next_test);
                      });

                      go_forward(aManager);
                    });
                  });
                });
              });
            });
          });

          go_forward(aManager);
        });

        go_back(aManager);
      });
    });
  });
});

// Tests that when displaying in-content and opened in the background the back
// and forward buttons still appear when switching tabs
add_test(function() {
  if (!gUseInContentUI) {
    run_next_test();
    return;
  }

  var tab = gBrowser.addTab("about:addons");
  var browser = gBrowser.getBrowserForTab(tab);

  browser.addEventListener("pageshow", function(event) {
    if (event.target.location.href != "about:addons")
      return;
    browser.removeEventListener("pageshow", arguments.callee, true);

    wait_for_manager_load(browser.contentWindow.wrappedJSObject, function() {
      wait_for_view_load(browser.contentWindow.wrappedJSObject, function(aManager) {
        gBrowser.selectedTab = tab;

        var doc = aManager.document;
        var btn = document.getElementById("back-button");
        if (!btn || is_hidden(btn)) {
          is_element_visible(doc.getElementById("back-btn"), "Back button should not be hidden");
          is_element_visible(doc.getElementById("forward-btn"), "Forward button should not be hidden");
        } else {
          is_element_hidden(doc.getElementById("back-btn"), "Back button should be hidden");
          is_element_hidden(doc.getElementById("forward-btn"), "Forward button should be hidden");
        }

        close_manager(aManager, run_next_test);
      });
    });
  }, true);
});

// Tests that refreshing the disicovery pane integrates properly with history
add_test(function() {
  open_manager("addons://list/plugin", function(aManager) {
    is_in_list(aManager, "addons://list/plugin", false, false);

    EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-discover"), { }, aManager);

    wait_for_view_load(aManager, function(aManager) {
      is_in_discovery(aManager, MAIN_URL, true, false);

      clickLink(aManager, "link-good", function() {
        is_in_discovery(aManager, SECOND_URL, true, false);

        EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-discover"), { }, aManager);
        
        waitForLoad(aManager, function() {
          is_in_discovery(aManager, MAIN_URL, true, false);

          go_back(aManager);

          waitForLoad(aManager, function() {
            is_in_discovery(aManager, SECOND_URL, true, true);

            go_back(aManager);

            waitForLoad(aManager, function() {
              is_in_discovery(aManager, MAIN_URL, true, true);

              go_back(aManager);

              wait_for_view_load(aManager, function(aManager) {
                is_in_list(aManager, "addons://list/plugin", false, true);

                go_forward(aManager);

                wait_for_view_load(aManager, function(aManager) {
                  is_in_discovery(aManager, MAIN_URL, true, true);

                  waitForLoad(aManager, function() {
                    is_in_discovery(aManager, SECOND_URL, true, true);

                    waitForLoad(aManager, function() {
                      is_in_discovery(aManager, MAIN_URL, true, false);

                      close_manager(aManager, run_next_test);
                    });
                    go_forward(aManager);
                  });

                  go_forward(aManager);
                });
              });
            });
          });
        });
      });
    });
  });
});
