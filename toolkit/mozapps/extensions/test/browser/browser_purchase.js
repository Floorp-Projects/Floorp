/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that marketplace results show up in searches, are sorted right and
// attempting to buy links through to the right webpage

const PREF_GETADDONS_GETSEARCHRESULTS = "extensions.getAddons.search.url";
const SEARCH_URL = TESTROOT + "browser_purchase.xml";

var gManagerWindow;

function test() {
  // Turn on searching for this test
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS, SEARCH_URL);

  waitForExplicitFinish();

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;

    waitForFocus(function() {
      var searchBox = gManagerWindow.document.getElementById("header-search");
      searchBox.value = "foo";

      EventUtils.synthesizeMouseAtCenter(searchBox, { }, gManagerWindow);
      EventUtils.synthesizeKey("VK_RETURN", { }, gManagerWindow);

      wait_for_view_load(gManagerWindow, function() {
        var remoteFilter = gManagerWindow.document.getElementById("search-filter-remote");
        EventUtils.synthesizeMouseAtCenter(remoteFilter, { }, gManagerWindow);

        run_next_test();
      });
    }, aWindow);
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    // Will have created an install so cancel it
    AddonManager.getAllInstalls(function(aInstalls) {
      is(aInstalls.length, 1, "Should have been one install created");
      aInstalls[0].cancel();

      finish();
    });
  });
}

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "anonid", anonid);
}

function get_install_btn(parent) {
  var installStatus = get_node(parent, "install-status");
  return get_node(installStatus, "install-remote-btn");
}

function get_purchase_btn(parent) {
  var installStatus = get_node(parent, "install-status");
  return get_node(installStatus, "purchase-remote-btn");
}

// Tests that the expected results appeared
add_test(function() {
  var list = gManagerWindow.document.getElementById("search-list");
  var items = Array.filter(list.childNodes, function(e) {
    return e.tagName == "richlistitem";
  });

  is(items.length, 5, "Should be 5 results");

  is(get_node(items[0], "name").value, "Ludicrously Expensive Add-on", "Add-on 0 should be in expected position");
  is_element_hidden(get_install_btn(items[0]), "Add-on 0 install button should be hidden");
  is_element_visible(get_purchase_btn(items[0]), "Add-on 0 purchase button should be visible");
  is(get_purchase_btn(items[0]).label, "Purchase for $101\u2026", "Add-on 0 should have the right price");

  is(get_node(items[1], "name").value, "Cheap Add-on", "Add-on 1 should be in expected position");
  is_element_hidden(get_install_btn(items[1]), "Add-on 1 install button should be hidden");
  is_element_visible(get_purchase_btn(items[1]), "Add-on 1 purchase button should be visible");
  is(get_purchase_btn(items[1]).label, "Purchase for $0.99\u2026", "Add-on 2 should have the right price");

  is(get_node(items[2], "name").value, "Reasonable Add-on", "Add-on 2 should be in expected position");
  is_element_hidden(get_install_btn(items[2]), "Add-on 2 install button should be hidden");
  is_element_visible(get_purchase_btn(items[2]), "Add-on 2 purchase button should be visible");
  is(get_purchase_btn(items[2]).label, "Purchase for $1\u2026", "Add-on 3 should have the right price");

  is(get_node(items[3], "name").value, "Free Add-on", "Add-on 3 should be in expected position");
  is_element_visible(get_install_btn(items[3]), "Add-on 3 install button should be visible");
  is_element_hidden(get_purchase_btn(items[3]), "Add-on 3 purchase button should be hidden");

  is(get_node(items[4], "name").value, "More Expensive Add-on", "Add-on 4 should be in expected position");
  is_element_hidden(get_install_btn(items[4]), "Add-on 4 install button should be hidden");
  is_element_visible(get_purchase_btn(items[4]), "Add-on 4 purchase button should be visible");
  is(get_purchase_btn(items[4]).label, "Purchase for $1.01\u2026", "Add-on 4 should have the right price");

  run_next_test();
});

// Tests that sorting by price works
add_test(function() {
  var list = gManagerWindow.document.getElementById("search-list");

  var sorters = gManagerWindow.document.getElementById("search-sorters");
  var priceSorter = get_node(sorters, "price-btn");
  info("Changing sort order");
  EventUtils.synthesizeMouseAtCenter(priceSorter, { }, gManagerWindow);

  var items = Array.filter(list.childNodes, function(e) {
    return e.tagName == "richlistitem";
  });

  is(get_node(items[0], "name").value, "Free Add-on", "Add-on 0 should be in expected position");
  is(get_node(items[1], "name").value, "Cheap Add-on", "Add-on 1 should be in expected position");
  is(get_node(items[2], "name").value, "Reasonable Add-on", "Add-on 2 should be in expected position");
  is(get_node(items[3], "name").value, "More Expensive Add-on", "Add-on 3 should be in expected position");
  is(get_node(items[4], "name").value, "Ludicrously Expensive Add-on", "Add-on 4 should be in expected position");

  info("Changing sort order");
  EventUtils.synthesizeMouseAtCenter(priceSorter, { }, gManagerWindow);

  var items = Array.filter(list.childNodes, function(e) {
    return e.tagName == "richlistitem";
  });

  is(get_node(items[0], "name").value, "Ludicrously Expensive Add-on", "Add-on 0 should be in expected position");
  is(get_node(items[1], "name").value, "More Expensive Add-on", "Add-on 1 should be in expected position");
  is(get_node(items[2], "name").value, "Reasonable Add-on", "Add-on 2 should be in expected position");
  is(get_node(items[3], "name").value, "Cheap Add-on", "Add-on 3 should be in expected position");
  is(get_node(items[4], "name").value, "Free Add-on", "Add-on 4 should be in expected position");

  run_next_test();
});

// Tests that clicking the buy button works from the list
add_test(function() {
  gBrowser.addEventListener("load", function(event) {
    if (!(event.target instanceof Document) ||
        event.target.location.href == "about:blank")
      return;
    gBrowser.removeEventListener("load", arguments.callee, true);

    is(gBrowser.currentURI.spec, TESTROOT + "releaseNotes.xhtml?addon5", "Should have loaded the right page");

    gBrowser.removeCurrentTab();

    if (gUseInContentUI) {
      is(gBrowser.currentURI.spec, "about:addons", "Should be back to the add-ons manager");
      run_next_test();
    }
    else {
      waitForFocus(run_next_test, gManagerWindow);
    }
  }, true);

  var list = gManagerWindow.document.getElementById("search-list");
  EventUtils.synthesizeMouseAtCenter(get_purchase_btn(list.firstChild), { }, gManagerWindow);
});

// Tests that clicking the buy button from the details view works
add_test(function() {
  gBrowser.addEventListener("load", function(event) {
    if (!(event.target instanceof Document) ||
        event.target.location.href == "about:blank")
      return;
    gBrowser.removeEventListener("load", arguments.callee, true);

    is(gBrowser.currentURI.spec, TESTROOT + "releaseNotes.xhtml?addon4", "Should have loaded the right page");

    gBrowser.removeCurrentTab();

    if (gUseInContentUI) {
      is(gBrowser.currentURI.spec, "about:addons", "Should be back to the add-ons manager");
      run_next_test();
    }
    else {
      waitForFocus(run_next_test, gManagerWindow);
    }
  }, true);

  var list = gManagerWindow.document.getElementById("search-list");
  var item = list.firstChild.nextSibling;
  list.ensureElementIsVisible(item);
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
  EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    var btn = gManagerWindow.document.getElementById("detail-purchase-btn");
    is_element_visible(btn, "Purchase button should be visible");

    EventUtils.synthesizeMouseAtCenter(btn, { }, gManagerWindow);
  });
});
