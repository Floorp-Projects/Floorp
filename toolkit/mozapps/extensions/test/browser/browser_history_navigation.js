/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint max-nested-callbacks: ["warn", 12] */

/**
 * Tests that history navigation works for the add-ons manager.
 */

// Request a longer timeout, because this tests run twice
// (once on XUL views and once on the HTML views).
requestLongerTimeout(4);

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

const MAIN_URL = `https://example.com/${RELATIVE_DIR}discovery.html`;
const DISCOAPI_URL = `http://example.com/${RELATIVE_DIR}/discovery/api_response_empty.json`;

// Clearing this pref is currently done from a cleanup function registered
// by the head.js file.
Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);

var gProvider = new MockProvider();
gProvider.createAddons([
  {
    id: "test1@tests.mozilla.org",
    name: "Test add-on 1",
    description: "foo",
  },
  {
    id: "test2@tests.mozilla.org",
    name: "Test add-on 2",
    description: "bar",
  },
  {
    id: "test3@tests.mozilla.org",
    name: "Test add-on 3",
    type: "theme",
    description: "bar",
  },
]);

function go_back() {
  gBrowser.goBack();
}

function go_back_backspace() {
  EventUtils.synthesizeKey("KEY_Backspace");
}

function go_forward_backspace() {
  EventUtils.synthesizeKey("KEY_Backspace", { shiftKey: true });
}

function go_forward() {
  gBrowser.goForward();
}

function check_state(canGoBack, canGoForward) {
  is(gBrowser.canGoBack, canGoBack, "canGoBack should be correct");
  is(gBrowser.canGoForward, canGoForward, "canGoForward should be correct");
}

function is_in_list(aManager, view, canGoBack, canGoForward) {
  var doc = aManager.document;
  var categoryUtils = new CategoryUtilities(aManager);

  is(
    categoryUtils.getSelectedViewId(),
    view,
    "Should be on the right category"
  );

  is(
    get_current_view(aManager).id,
    "html-view",
    "the current view should be set to the HTML about:addons browser"
  );
  doc = aManager.getHtmlBrowser().contentDocument;
  ok(
    doc.querySelector("addon-list"),
    "Got a list-view in the HTML about:addons browser"
  );

  check_state(canGoBack, canGoForward);
}

function is_in_detail(aManager, view, canGoBack, canGoForward) {
  var doc = aManager.document;
  var categoryUtils = new CategoryUtilities(aManager);

  is(
    categoryUtils.getSelectedViewId(),
    view,
    "Should be on the right category"
  );

  is(
    get_current_view(aManager).id,
    "html-view",
    "the current view should be set to the HTML about:addons browser"
  );
  doc = aManager.getHtmlBrowser().contentDocument;
  is(
    doc.querySelectorAll("addon-card").length,
    1,
    "Got a detail-view in the HTML about:addons browser"
  );

  check_state(canGoBack, canGoForward);
}

function is_in_discovery(aManager, url, canGoBack, canGoForward) {
  is(
    get_current_view(aManager).id,
    "html-view",
    "the current view should be set to the HTML about:addons browser"
  );
  const doc = aManager.getHtmlBrowser().contentDocument;
  ok(
    doc.querySelector("discovery-pane"),
    "Got a discovery panel in the HTML about:addons browser"
  );

  check_state(canGoBack, canGoForward);
}

async function expand_addon_element(aManager, aId) {
  var addon = get_addon_element(aManager, aId);
  addon.click();
}

function wait_for_page_load(browser) {
  return BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");
}

// Tests simple forward and back navigation and that the right heading and
// category is selected
add_task(async function test_navigate_history() {
  let aManager = await open_manager("addons://list/extension");
  let categoryUtils = new CategoryUtilities(aManager);
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  EventUtils.synthesizeMouseAtCenter(categoryUtils.get("plugin"), {}, aManager);

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  info("Part 3");
  is_in_list(aManager, "addons://list/extension", false, true);

  go_forward();

  aManager = await wait_for_view_load(aManager);
  info("Part 4");
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  info("Part 5");
  is_in_list(aManager, "addons://list/extension", false, true);

  await expand_addon_element(aManager, "test1@tests.mozilla.org");

  aManager = await wait_for_view_load(aManager);
  info("Part 6");
  is_in_detail(aManager, "addons://list/extension", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  info("Part 7");
  is_in_list(aManager, "addons://list/extension", false, true);

  await close_manager(aManager);
});

// Tests that browsing to the add-ons manager from a website and going back works
add_task(async function test_navigate_between_webpage_and_aboutaddons() {
  info("Part 1");
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/",
    true,
    true
  );

  info("Part 2");
  ok(!gBrowser.canGoBack, "Should not be able to go back");
  ok(!gBrowser.canGoForward, "Should not be able to go forward");

  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:addons");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  let manager = await wait_for_manager_load(gBrowser.contentWindow);

  info("Part 3");
  is_in_list(manager, "addons://list/extension", true, false);

  // XXX: This is less than ideal, as it's currently difficult to deal with
  // the browser frame switching between remote/non-remote in e10s mode.
  let promiseLoaded;
  if (gMultiProcessBrowser) {
    promiseLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  } else {
    promiseLoaded = BrowserTestUtils.waitForEvent(
      gBrowser.selectedBrowser,
      "pageshow"
    );
  }

  go_back(manager);
  await promiseLoaded;

  info("Part 4");
  is(
    gBrowser.currentURI.spec,
    "http://example.com/",
    "Should be showing the webpage"
  );
  ok(!gBrowser.canGoBack, "Should not be able to go back");
  ok(gBrowser.canGoForward, "Should be able to go forward");

  promiseLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  go_forward(manager);
  await promiseLoaded;

  manager = gBrowser.selectedBrowser.contentWindow;
  info("Part 5");
  is_in_list(manager, "addons://list/extension", true, false);

  await close_manager(manager);
});

// Tests simple forward and back navigation and that the right heading and
// category is selected -- Keyboard navigation [Bug 565359]
// Only add the test if the backspace key navigates back and addon-manager
// loaded in a tab
add_task(async function test_keyboard_history_navigation() {
  if (Services.prefs.getIntPref("browser.backspace_action") != 0) {
    info("Test skipped on browser.backspace_action != 0");
    return;
  }

  let aManager = await open_manager("addons://list/extension");
  let categoryUtils = new CategoryUtilities(aManager);
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  EventUtils.synthesizeMouseAtCenter(categoryUtils.get("plugin"), {}, aManager);

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back_backspace();

  aManager = await wait_for_view_load(aManager);
  info("Part 3");
  is_in_list(aManager, "addons://list/extension", false, true);

  go_forward_backspace();

  aManager = await wait_for_view_load(aManager);
  info("Part 4");
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back_backspace();

  aManager = await wait_for_view_load(aManager);
  info("Part 5");
  is_in_list(aManager, "addons://list/extension", false, true);

  await expand_addon_element(aManager, "test1@tests.mozilla.org");

  aManager = await wait_for_view_load(aManager);
  info("Part 6");
  is_in_detail(aManager, "addons://list/extension", true, false);

  go_back_backspace();

  aManager = await wait_for_view_load(aManager);
  info("Part 7");
  is_in_list(aManager, "addons://list/extension", false, true);

  await close_manager(aManager);
});

// Tests that opening a custom first view only stores a single history entry
add_task(async function test_single_history_entry() {
  let aManager = await open_manager("addons://list/plugin");
  let categoryUtils = new CategoryUtilities(aManager);
  info("Part 1");
  is_in_list(aManager, "addons://list/plugin", false, false);

  EventUtils.synthesizeMouseAtCenter(
    categoryUtils.get("extension"),
    {},
    aManager
  );

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_list(aManager, "addons://list/extension", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  info("Part 3");
  is_in_list(aManager, "addons://list/plugin", false, true);

  await close_manager(aManager);
});

// Tests that opening a view while the manager is already open adds a new
// history entry
add_task(async function test_new_history_entry_while_opened() {
  let aManager = await open_manager("addons://list/extension");
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  aManager.loadView("addons://list/plugin");

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  info("Part 3");
  is_in_list(aManager, "addons://list/extension", false, true);

  go_forward();

  aManager = await wait_for_view_load(aManager);
  info("Part 4");
  is_in_list(aManager, "addons://list/plugin", true, false);

  await close_manager(aManager);
});

// Tests than navigating to a website and then going back returns to the
// previous view
add_task(async function test_navigate_back_from_website() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.allow_eval_with_system_principal", true]],
  });

  let aManager = await open_manager("addons://list/plugin");
  info("Part 1");
  is_in_list(aManager, "addons://list/plugin", false, false);

  BrowserTestUtils.loadURI(gBrowser, "http://example.com/");
  await wait_for_page_load(gBrowser.selectedBrowser);

  info("Part 2");

  await new Promise(resolve =>
    executeSoon(function() {
      ok(gBrowser.canGoBack, "Should be able to go back");
      ok(!gBrowser.canGoForward, "Should not be able to go forward");

      go_back();

      gBrowser.addEventListener("pageshow", async function listener(event) {
        if (event.target.location != "about:addons") {
          return;
        }
        gBrowser.removeEventListener("pageshow", listener);

        aManager = await wait_for_view_load(
          gBrowser.contentWindow.wrappedJSObject
        );
        info("Part 3");
        is_in_list(aManager, "addons://list/plugin", false, true);

        executeSoon(() => go_forward());
        wait_for_page_load(gBrowser.selectedBrowser).then(() => {
          info("Part 4");

          executeSoon(function() {
            ok(gBrowser.canGoBack, "Should be able to go back");
            ok(!gBrowser.canGoForward, "Should not be able to go forward");

            go_back();

            gBrowser.addEventListener("pageshow", async function listener(
              event
            ) {
              if (event.target.location != "about:addons") {
                return;
              }
              gBrowser.removeEventListener("pageshow", listener);
              aManager = await wait_for_view_load(
                gBrowser.contentWindow.wrappedJSObject
              );
              info("Part 5");
              is_in_list(aManager, "addons://list/plugin", false, true);

              resolve();
            });
          });
        });
      });
    })
  );

  await close_manager(aManager);
  await SpecialPowers.popPrefEnv();
});

// Tests that refreshing a list view does not affect the history
add_task(async function test_refresh_listview_donot_add_history_entries() {
  let aManager = await open_manager("addons://list/extension");
  let categoryUtils = new CategoryUtilities(aManager);
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  EventUtils.synthesizeMouseAtCenter(categoryUtils.get("plugin"), {}, aManager);

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_list(aManager, "addons://list/plugin", true, false);

  await new Promise(resolve => {
    gBrowser.reload();
    gBrowser.addEventListener("pageshow", async function listener(event) {
      if (event.target.location != "about:addons") {
        return;
      }
      gBrowser.removeEventListener("pageshow", listener);

      aManager = await wait_for_view_load(
        gBrowser.contentWindow.wrappedJSObject
      );
      info("Part 3");
      is_in_list(aManager, "addons://list/plugin", true, false);

      go_back();
      aManager = await wait_for_view_load(aManager);
      info("Part 4");
      is_in_list(aManager, "addons://list/extension", false, true);
      resolve();
    });
  });

  await close_manager(aManager);
});

// Tests that refreshing a detail view does not affect the history
add_task(async function test_refresh_detailview_donot_add_history_entries() {
  let aManager = await open_manager(null);
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  await expand_addon_element(aManager, "test1@tests.mozilla.org");

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_detail(aManager, "addons://list/extension", true, false);

  await new Promise(resolve => {
    gBrowser.reload();
    gBrowser.addEventListener("pageshow", async function listener(event) {
      if (event.target.location != "about:addons") {
        return;
      }
      gBrowser.removeEventListener("pageshow", listener);

      aManager = await wait_for_view_load(
        gBrowser.contentWindow.wrappedJSObject
      );
      info("Part 3");
      is_in_detail(aManager, "addons://list/extension", true, false);

      go_back();
      aManager = await wait_for_view_load(aManager);
      info("Part 4");
      is_in_list(aManager, "addons://list/extension", false, true);
      resolve();
    });
  });

  await close_manager(aManager);
});

// Tests that removing an extension from the detail view goes back and doesn't
// allow you to go forward again.
add_task(async function test_history_on_detailview_extension_removed() {
  let aManager = await open_manager("addons://list/extension");

  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  await expand_addon_element(aManager, "test1@tests.mozilla.org");

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_detail(aManager, "addons://list/extension", true, false);

  const doc = aManager.getHtmlBrowser().contentDocument;
  const addonCard = doc.querySelector(
    'addon-card[addon-id="test1@tests.mozilla.org"]'
  );
  const promptService = mockPromptService();
  promptService._response = 0;
  addonCard.querySelector("[action=remove]").click();

  await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/extension", true, false);

  const addon = await AddonManager.getAddonByID("test1@tests.mozilla.org");
  addon.cancelUninstall();

  await close_manager(aManager);
});

// Tests that opening the manager opens the last view
add_task(async function test_open_last_view() {
  let aManager = await open_manager("addons://list/plugin");
  info("Part 1");
  is_in_list(aManager, "addons://list/plugin", false, false);

  await close_manager(aManager);
  aManager = await open_manager(null);
  info("Part 2");
  is_in_list(aManager, "addons://list/plugin", false, false);

  await close_manager(aManager);
});

// Tests that navigating the discovery page works when that was the first view
add_task(async function test_discopane_first_history_entry() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.getAddons.discovery.api_url", DISCOAPI_URL]],
  });

  let aManager = await open_manager("addons://discover/");
  let categoryUtils = new CategoryUtilities(aManager);
  info("1");
  is_in_discovery(aManager, MAIN_URL, false, false);

  EventUtils.synthesizeMouseAtCenter(categoryUtils.get("plugin"), {}, aManager);

  aManager = await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back();
  aManager = await wait_for_view_load(aManager);

  is_in_discovery(aManager, MAIN_URL, false, true);

  await close_manager(aManager);
});

// Tests that navigating the discovery page works when that was the second view
add_task(async function test_discopane_second_history_entry() {
  let aManager = await open_manager("addons://list/plugin");
  let categoryUtils = new CategoryUtilities(aManager);
  is_in_list(aManager, "addons://list/plugin", false, false);

  EventUtils.synthesizeMouseAtCenter(
    categoryUtils.get("discover"),
    {},
    aManager
  );

  aManager = await wait_for_view_load(aManager);
  is_in_discovery(aManager, MAIN_URL, true, false);

  EventUtils.synthesizeMouseAtCenter(categoryUtils.get("plugin"), {}, aManager);

  aManager = await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  is_in_discovery(aManager, MAIN_URL, true, true);

  go_back();

  aManager = await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/plugin", false, true);

  await close_manager(aManager);
});

add_task(async function test_initialSelectedView_on_aboutaddons_reload() {
  let managerWindow = await open_manager("addons://list/extension");
  ok(
    managerWindow.gViewController.initialViewSelected,
    "initialViewSelected is true as expected on first about:addons load"
  );

  managerWindow.location.reload();
  await wait_for_manager_load(managerWindow);
  await wait_for_view_load(managerWindow);

  ok(
    managerWindow.gViewController.initialViewSelected,
    "initialViewSelected is true as expected on first about:addons load"
  );
  is(managerWindow.gPendingInitializations, 0, "No pending initializations");

  await close_manager(managerWindow);
});
