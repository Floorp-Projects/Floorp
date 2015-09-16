/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 591465 - Context menu of add-ons miss context related state change entries


var tempScope = {};
Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", tempScope);
var LightweightThemeManager = tempScope.LightweightThemeManager;


const PREF_GETADDONS_MAXRESULTS = "extensions.getAddons.maxResults";
const PREF_GETADDONS_GETSEARCHRESULTS = "extensions.getAddons.search.url";
const SEARCH_URL = TESTROOT + "browser_bug591465.xml";
const SEARCH_QUERY = "SEARCH";

var gManagerWindow;
var gProvider;
var gContextMenu;
var gLWTheme = {
                id: "4",
                version: "1",
                name: "Bling",
                description: "SO MUCH BLING!",
                author: "Pixel Pusher",
                homepageURL: "http://mochi.test:8888/data/index.html",
                headerURL: "http://mochi.test:8888/data/header.png",
                footerURL: "http://mochi.test:8888/data/footer.png",
                previewURL: "http://mochi.test:8888/data/preview.png",
                iconURL: "http://mochi.test:8888/data/icon.png"
              };


function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "addon 1",
    version: "1.0"
  }, {
    id: "addon2@tests.mozilla.org",
    name: "addon 2",
    version: "1.0",
    _userDisabled: true
  }, {
    id: "theme1@tests.mozilla.org",
    name: "theme 1",
    version: "1.0",
    type: "theme"
  }, {
    id: "theme2@tests.mozilla.org",
    name: "theme 2",
    version: "1.0",
    type: "theme",
    _userDisabled: true
   }, {
    id: "theme3@tests.mozilla.org",
    name: "theme 3",
    version: "1.0",
    type: "theme",
    permissions: 0
  }]);


  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gContextMenu = aWindow.document.getElementById("addonitem-popup");
    run_next_test();
  });
}


function end_test() {
  close_manager(gManagerWindow, finish);
}


function check_contextmenu(aIsTheme, aIsEnabled, aIsRemote, aIsDetails, aIsSingleItemCase) {
  if (aIsTheme || aIsEnabled || aIsRemote)
    is_element_hidden(gManagerWindow.document.getElementById("menuitem_enableItem"),
                       "'Enable' should be hidden");
  else
    is_element_visible(gManagerWindow.document.getElementById("menuitem_enableItem"),
                       "'Enable' should be visible");

  if (aIsTheme || !aIsEnabled || aIsRemote)
    is_element_hidden(gManagerWindow.document.getElementById("menuitem_disableItem"),
                       "'Disable' should be hidden");
  else
    is_element_visible(gManagerWindow.document.getElementById("menuitem_disableItem"),
                       "'Disable' should be visible");

  if (!aIsTheme || aIsEnabled || aIsRemote || aIsSingleItemCase)
    is_element_hidden(gManagerWindow.document.getElementById("menuitem_enableTheme"),
                       "'Wear Theme' should be hidden");
  else
    is_element_visible(gManagerWindow.document.getElementById("menuitem_enableTheme"),
                       "'Wear Theme' should be visible");

  if (!aIsTheme || !aIsEnabled || aIsRemote || aIsSingleItemCase)
    is_element_hidden(gManagerWindow.document.getElementById("menuitem_disableTheme"),
                       "'Stop Wearing Theme' should be hidden");
  else
    is_element_visible(gManagerWindow.document.getElementById("menuitem_disableTheme"),
                       "'Stop Wearing Theme' should be visible");

  if (aIsRemote)
    is_element_visible(gManagerWindow.document.getElementById("menuitem_installItem"),
                       "'Install' should be visible");
  else
    is_element_hidden(gManagerWindow.document.getElementById("menuitem_installItem"),
                       "'Install' should be hidden");

  if (aIsDetails)
    is_element_hidden(gManagerWindow.document.getElementById("menuitem_showDetails"),
                       "'Show More Information' should be hidden in details view");
  else
    is_element_visible(gManagerWindow.document.getElementById("menuitem_showDetails"),
                       "'Show More Information' should be visible in list view");

  if (aIsSingleItemCase)
    is_element_hidden(gManagerWindow.document.getElementById("addonitem-menuseparator"),
                       "Menu separator should be hidden with only one menu item");
  else
    is_element_visible(gManagerWindow.document.getElementById("addonitem-menuseparator"),
                       "Menu separator should be visible with multiple menu items");

}


add_test(function() {
  var el = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  isnot(el, null, "Should have found addon element");

  gContextMenu.addEventListener("popupshown", function() {
    gContextMenu.removeEventListener("popupshown", arguments.callee, false);

    check_contextmenu(false, true, false, false, false);

    gContextMenu.hidePopup();
    run_next_test();
  }, false);

  info("Opening context menu on enabled extension item");
  el.parentNode.ensureElementIsVisible(el);
  EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
  EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
});

add_test(function() {
  var el = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  isnot(el, null, "Should have found addon element");
  el.mAddon.userDisabled = true;

  gContextMenu.addEventListener("popupshown", function() {
    gContextMenu.removeEventListener("popupshown", arguments.callee, false);

    check_contextmenu(false, false, false, false, false);

    gContextMenu.hidePopup();
    run_next_test();
  }, false);

  info("Opening context menu on newly disabled extension item");
  el.parentNode.ensureElementIsVisible(el);
  EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
  EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
});

add_test(function() {
  var el = get_addon_element(gManagerWindow, "addon1@tests.mozilla.org");
  isnot(el, null, "Should have found addon element");
  el.mAddon.userDisabled = false;

  gContextMenu.addEventListener("popupshown", function() {
    gContextMenu.removeEventListener("popupshown", arguments.callee, false);

    check_contextmenu(false, true, false, false, false);

    gContextMenu.hidePopup();
    run_next_test();
  }, false);

  info("Opening context menu on newly enabled extension item");
  el.parentNode.ensureElementIsVisible(el);
  EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
  EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
});

add_test(function() {
  var el = get_addon_element(gManagerWindow, "addon2@tests.mozilla.org");

  gContextMenu.addEventListener("popupshown", function() {
    gContextMenu.removeEventListener("popupshown", arguments.callee, false);

    check_contextmenu(false, false, false, false, false);

    gContextMenu.hidePopup();
    run_next_test();
  }, false);

  info("Opening context menu on disabled extension item");
  el.parentNode.ensureElementIsVisible(el);
  EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
  EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
});


add_test(function() {
  gManagerWindow.loadView("addons://list/theme");
  wait_for_view_load(gManagerWindow, function() {
    var el = get_addon_element(gManagerWindow, "theme1@tests.mozilla.org");

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(true, true, false, false, false);

      gContextMenu.hidePopup();
      run_next_test();
    }, false);

    info("Opening context menu on enabled theme item");
    el.parentNode.ensureElementIsVisible(el);
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});


add_test(function() {
  var el = get_addon_element(gManagerWindow, "theme2@tests.mozilla.org");

  gContextMenu.addEventListener("popupshown", function() {
    gContextMenu.removeEventListener("popupshown", arguments.callee, false);

    check_contextmenu(true, false, false, false, false);

    gContextMenu.hidePopup();
    run_next_test();
  }, false);

  info("Opening context menu on disabled theme item");
  el.parentNode.ensureElementIsVisible(el);
  EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
  EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
});


add_test(function() {
  LightweightThemeManager.currentTheme = gLWTheme;

  var el = get_addon_element(gManagerWindow, "4@personas.mozilla.org");

  gContextMenu.addEventListener("popupshown", function() {
    gContextMenu.removeEventListener("popupshown", arguments.callee, false);

    check_contextmenu(true, true, false, false, false);

    gContextMenu.hidePopup();
    run_next_test();
  }, false);

  info("Opening context menu on enabled LW theme item");
  el.parentNode.ensureElementIsVisible(el);
  EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
  EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
});


add_test(function() {
  LightweightThemeManager.currentTheme = null;

  var el = get_addon_element(gManagerWindow, "4@personas.mozilla.org");

  gContextMenu.addEventListener("popupshown", function() {
    gContextMenu.removeEventListener("popupshown", arguments.callee, false);

    check_contextmenu(true, false, false, false, false);

    gContextMenu.hidePopup();
    run_next_test();
  }, false);

  info("Opening context menu on disabled LW theme item");
  el.parentNode.ensureElementIsVisible(el);
  EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
  EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
});


add_test(function() {
  LightweightThemeManager.currentTheme = gLWTheme;

  gManagerWindow.loadView("addons://detail/4@personas.mozilla.org");
  wait_for_view_load(gManagerWindow, function() {

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(true, true, false, true, false);

      gContextMenu.hidePopup();
      run_next_test();
    }, false);

    info("Opening context menu on enabled LW theme, in detail view");
    var el = gManagerWindow.document.querySelector("#detail-view .detail-view-container");
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});


add_test(function() {
  LightweightThemeManager.currentTheme = null;

  gManagerWindow.loadView("addons://detail/4@personas.mozilla.org");
  wait_for_view_load(gManagerWindow, function() {

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(true, false, false, true, false);

      gContextMenu.hidePopup();

      AddonManager.getAddonByID("4@personas.mozilla.org", function(aAddon) {
        aAddon.uninstall();
        run_next_test();
      });
    }, false);

    info("Opening context menu on disabled LW theme, in detail view");
    var el = gManagerWindow.document.querySelector("#detail-view .detail-view-container");
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});


add_test(function() {
  gManagerWindow.loadView("addons://detail/addon1@tests.mozilla.org");
  wait_for_view_load(gManagerWindow, function() {

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(false, true, false, true, false);

      gContextMenu.hidePopup();
      run_next_test();
    }, false);

    info("Opening context menu on enabled extension, in detail view");
    var el = gManagerWindow.document.querySelector("#detail-view .detail-view-container");
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});


add_test(function() {
  gManagerWindow.loadView("addons://detail/addon2@tests.mozilla.org");
  wait_for_view_load(gManagerWindow, function() {

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(false, false, false, true, false);

      gContextMenu.hidePopup();
      run_next_test();
    }, false);

    info("Opening context menu on disabled extension, in detail view");
    var el = gManagerWindow.document.querySelector("#detail-view .detail-view-container");
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});


add_test(function() {
  gManagerWindow.loadView("addons://detail/theme1@tests.mozilla.org");
  wait_for_view_load(gManagerWindow, function() {

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(true, true, false, true, false);

      gContextMenu.hidePopup();
      run_next_test();
    }, false);

    info("Opening context menu on enabled theme, in detail view");
    var el = gManagerWindow.document.querySelector("#detail-view .detail-view-container");
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});


add_test(function() {
  gManagerWindow.loadView("addons://detail/theme2@tests.mozilla.org");
  wait_for_view_load(gManagerWindow, function() {

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(true, false, false, true, false);

      gContextMenu.hidePopup();
      run_next_test();
    }, false);

    info("Opening context menu on disabled theme, in detail view");
    var el = gManagerWindow.document.querySelector("#detail-view .detail-view-container");
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});

add_test(function() {
  gManagerWindow.loadView("addons://detail/theme3@tests.mozilla.org");
  wait_for_view_load(gManagerWindow, function() {

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(true, true, false, true, true);

      gContextMenu.hidePopup();
      run_next_test();
    }, false);

    info("Opening context menu with single menu item on enabled theme, in detail view");
    var el = gManagerWindow.document.querySelector("#detail-view .detail-view-container");
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});

add_test(function() {
  info("Searching for remote addons");

  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS, SEARCH_URL);
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);

  var searchBox = gManagerWindow.document.getElementById("header-search");
  searchBox.value = SEARCH_QUERY;

  EventUtils.synthesizeMouseAtCenter(searchBox, { }, gManagerWindow);
  EventUtils.synthesizeKey("VK_RETURN", { }, gManagerWindow);

  wait_for_view_load(gManagerWindow, function() {
    var filter = gManagerWindow.document.getElementById("search-filter-remote");
    EventUtils.synthesizeMouseAtCenter(filter, { }, gManagerWindow);
    executeSoon(function() {

      var el = get_addon_element(gManagerWindow, "remote1@tests.mozilla.org");

      gContextMenu.addEventListener("popupshown", function() {
        gContextMenu.removeEventListener("popupshown", arguments.callee, false);

        check_contextmenu(false, false, true, false, false);

        gContextMenu.hidePopup();
        run_next_test();
      }, false);

      info("Opening context menu on remote extension item");
      el.parentNode.ensureElementIsVisible(el);
      EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
      EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);

    });
  });
});


add_test(function() {
  gManagerWindow.loadView("addons://detail/remote1@tests.mozilla.org");
  wait_for_view_load(gManagerWindow, function() {

    gContextMenu.addEventListener("popupshown", function() {
      gContextMenu.removeEventListener("popupshown", arguments.callee, false);

      check_contextmenu(false, false, true, true, false);

      gContextMenu.hidePopup();

      // Delete the created install
      AddonManager.getAllInstalls(function(aInstalls) {
        is(aInstalls.length, 1, "Should be one available install");
        aInstalls[0].cancel();

        run_next_test();
      });
    }, false);

    info("Opening context menu on remote extension, in detail view");
    var el = gManagerWindow.document.querySelector("#detail-view .detail-view-container");
    EventUtils.synthesizeMouse(el, 4, 4, { }, gManagerWindow);
    EventUtils.synthesizeMouse(el, 4, 4, { type: "contextmenu", button: 2 }, gManagerWindow);
  });
});
