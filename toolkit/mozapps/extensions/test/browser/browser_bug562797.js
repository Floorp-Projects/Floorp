/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint max-nested-callbacks: ["warn", 12] */

/**
 * Tests that history navigation works for the add-ons manager.
 */

const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery.html";
const SECOND_URL = "https://example.com/" + RELATIVE_DIR + "releaseNotes.xhtml";

var gLoadCompleteCallback = null;

var gProgressListener = {
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    // Only care about the network stop status events
    if (!(aStateFlags & (Ci.nsIWebProgressListener.STATE_IS_NETWORK)) ||
        !(aStateFlags & (Ci.nsIWebProgressListener.STATE_STOP)))
      return;

    if (gLoadCompleteCallback)
      executeSoon(gLoadCompleteCallback);
    gLoadCompleteCallback = null;
  },

  onLocationChange() { },
  onSecurityChange() { },
  onProgressChange() { },
  onStatusChange() { },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener,
                                          Ci.nsISupportsWeakReference]),
};

function waitForLoad(aManager, aCallback) {
  let promise = new Promise(resolve => {
    var browser = aManager.document.getElementById("discover-browser");
    browser.addProgressListener(gProgressListener);

    gLoadCompleteCallback = function() {
      browser.removeProgressListener(gProgressListener);
      resolve();
    };
  });
  if (aCallback) {
    promise.then(aCallback);
  }
  return promise;
}

function clickLink(aManager, aId, aCallback) {
  let promise = new Promise(async resolve => {
    waitForLoad(aManager, resolve);

    var browser = aManager.document.getElementById("discover-browser");

    var link = browser.contentDocument.getElementById(aId);
    EventUtils.sendMouseEvent({type: "click"}, link);
  });
  if (aCallback) {
    promise.then(aCallback);
  }
  return promise;
}

function test() {
  requestLongerTimeout(2);

  waitForExplicitFinish();

  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);

  SpecialPowers.pushPrefEnv({"set": [
      ["dom.ipc.processCount", 1],
    ]}, () => {
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
  });

  run_next_test();
}

function end_test() {
  finish();
}

function go_back() {
  gBrowser.goBack();
}

function go_back_backspace() {
    EventUtils.synthesizeKey("KEY_Backspace");
}

function go_forward_backspace() {
    EventUtils.synthesizeKey("KEY_Backspace", {shiftKey: true});
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

  is(doc.getElementById("categories").selectedItem.value, view, "Should be on the right category");
  is(get_current_view(aManager).id, "list-view", "Should be on the right view");

  check_state(canGoBack, canGoForward);
}

function is_in_detail(aManager, view, canGoBack, canGoForward) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, view, "Should be on the right category");
  is(get_current_view(aManager).id, "detail-view", "Should be on the right view");

  check_state(canGoBack, canGoForward);
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

  check_state(canGoBack, canGoForward);
}

function double_click_addon_element(aManager, aId) {
  var addon = get_addon_element(aManager, aId);
  addon.parentNode.ensureElementIsVisible(addon);
  EventUtils.synthesizeMouseAtCenter(addon, { clickCount: 1 }, aManager);
  EventUtils.synthesizeMouseAtCenter(addon, { clickCount: 2 }, aManager);
}

// Tests simple forward and back navigation and that the right heading and
// category is selected
add_test(async function() {
  let aManager = await open_manager("addons://list/extension");
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

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

  double_click_addon_element(aManager, "test1@tests.mozilla.org");

  aManager = await wait_for_view_load(aManager);
  info("Part 6");
  is_in_detail(aManager, "addons://list/extension", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  info("Part 7");
  is_in_list(aManager, "addons://list/extension", false, true);

  close_manager(aManager, run_next_test);
});

// Tests that browsing to the add-ons manager from a website and going back works
add_test(function() {

  function promiseManagerLoaded(manager) {
    return new Promise(resolve => {
      wait_for_manager_load(manager, resolve);
    });
  }

  (async function() {
    info("Part 1");
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/", true, true);

    info("Part 2");
    ok(!gBrowser.canGoBack, "Should not be able to go back");
    ok(!gBrowser.canGoForward, "Should not be able to go forward");

    await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:addons");
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    let manager = await promiseManagerLoaded(gBrowser.contentWindow.wrappedJSObject);

    info("Part 3");
    is_in_list(manager, "addons://list/extension", true, false);

    // XXX: This is less than ideal, as it's currently difficult to deal with
    // the browser frame switching between remote/non-remote in e10s mode.
    let promiseLoaded;
    if (gMultiProcessBrowser) {
      promiseLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    } else {
      promiseLoaded = BrowserTestUtils.waitForEvent(gBrowser.selectedBrowser, "pageshow");
    }

    go_back(manager);
    await promiseLoaded;

    info("Part 4");
    is(gBrowser.currentURI.spec, "http://example.com/", "Should be showing the webpage");
    ok(!gBrowser.canGoBack, "Should not be able to go back");
    ok(gBrowser.canGoForward, "Should be able to go forward");

    promiseLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    go_forward(manager);
    await promiseLoaded;

    manager = await promiseManagerLoaded(gBrowser.contentWindow.wrappedJSObject);
    info("Part 5");
    is_in_list(manager, "addons://list/extension", true, false);

    close_manager(manager, run_next_test);
  })();
});

// Tests simple forward and back navigation and that the right heading and
// category is selected -- Keyboard navigation [Bug 565359]
// Only add the test if the backspace key navigates back and addon-manager
// loaded in a tab
add_test(async function() {

  if (Services.prefs.getIntPref("browser.backspace_action") != 0) {
    run_next_test();
    return;
  }

  let aManager = await open_manager("addons://list/extension");
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

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

  double_click_addon_element(aManager, "test1@tests.mozilla.org");

  aManager = await wait_for_view_load(aManager);
  info("Part 6");
  is_in_detail(aManager, "addons://list/extension", true, false);

  go_back_backspace();

  aManager = await wait_for_view_load(aManager);
  info("Part 7");
  is_in_list(aManager, "addons://list/extension", false, true);

  close_manager(aManager, run_next_test);
});


// Tests that opening a custom first view only stores a single history entry
add_test(async function() {
  let aManager = await open_manager("addons://list/plugin");
  info("Part 1");
  is_in_list(aManager, "addons://list/plugin", false, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-extension"), { }, aManager);

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_list(aManager, "addons://list/extension", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  info("Part 3");
  is_in_list(aManager, "addons://list/plugin", false, true);

  close_manager(aManager, run_next_test);
});


// Tests that opening a view while the manager is already open adds a new
// history entry
add_test(async function() {
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

  close_manager(aManager, run_next_test);
});

function wait_for_page_show(browser) {
  let promise = new Promise(resolve => {
    let removeFunc;
    let listener = () => {
      removeFunc();
      resolve();
    };
    removeFunc = BrowserTestUtils.addContentEventListener(browser, "pageshow", listener, false,
                                                          (event) => event.target.location == "http://example.com/",
                                                          false, false);
  });
  return promise;
}

// Tests than navigating to a website and then going back returns to the
// previous view
add_test(async function() {

  let aManager = await open_manager("addons://list/plugin");
  info("Part 1");
  is_in_list(aManager, "addons://list/plugin", false, false);

  gBrowser.loadURI("http://example.com/");
  wait_for_page_show(gBrowser.selectedBrowser).then(() => {
    info("Part 2");

    executeSoon(function() {
      ok(gBrowser.canGoBack, "Should be able to go back");
      ok(!gBrowser.canGoForward, "Should not be able to go forward");

      go_back();

      gBrowser.addEventListener("pageshow", async function listener(event) {
        if (event.target.location != "about:addons")
          return;
        gBrowser.removeEventListener("pageshow", listener);

        aManager = await wait_for_view_load(gBrowser.contentWindow.wrappedJSObject);
        info("Part 3");
        is_in_list(aManager, "addons://list/plugin", false, true);

        executeSoon(() => go_forward());
        wait_for_page_show(gBrowser.selectedBrowser).then(() => {
          info("Part 4");

          executeSoon(function() {
            ok(gBrowser.canGoBack, "Should be able to go back");
            ok(!gBrowser.canGoForward, "Should not be able to go forward");

            go_back();

            gBrowser.addEventListener("pageshow", async function listener(event) {
              if (event.target.location != "about:addons")
                return;
              gBrowser.removeEventListener("pageshow", listener);
              aManager = await wait_for_view_load(gBrowser.contentWindow.wrappedJSObject);
              info("Part 5");
              is_in_list(aManager, "addons://list/plugin", false, true);

              close_manager(aManager, run_next_test);
            });
          });
        });
      });
    });
  });
});

// Tests that refreshing a list view does not affect the history
add_test(async function() {

  let aManager = await open_manager("addons://list/extension");
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_list(aManager, "addons://list/plugin", true, false);

  gBrowser.reload();
  gBrowser.addEventListener("pageshow", async function listener(event) {
    if (event.target.location != "about:addons")
      return;
    gBrowser.removeEventListener("pageshow", listener);

    aManager = await wait_for_view_load(gBrowser.contentWindow.wrappedJSObject);
    info("Part 3");
    is_in_list(aManager, "addons://list/plugin", true, false);

    go_back();
    aManager = await wait_for_view_load(aManager);
    info("Part 4");
    is_in_list(aManager, "addons://list/extension", false, true);

    close_manager(aManager, run_next_test);
  });
});

// Tests that refreshing a detail view does not affect the history
add_test(async function() {

  let aManager = await open_manager(null);
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  double_click_addon_element(aManager, "test1@tests.mozilla.org");

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_detail(aManager, "addons://list/extension", true, false);

  gBrowser.reload();
  gBrowser.addEventListener("pageshow", async function listener(event) {
    if (event.target.location != "about:addons")
      return;
    gBrowser.removeEventListener("pageshow", listener);

    aManager = await wait_for_view_load(gBrowser.contentWindow.wrappedJSObject);
    info("Part 3");
    is_in_detail(aManager, "addons://list/extension", true, false);

    go_back();
    aManager = await wait_for_view_load(aManager);
    info("Part 4");
    is_in_list(aManager, "addons://list/extension", false, true);

    close_manager(aManager, run_next_test);
  });
});

// Tests that removing an extension from the detail view goes back and doesn't
// allow you to go forward again.
add_test(async function() {
  let aManager = await open_manager("addons://list/extension");
  info("Part 1");
  is_in_list(aManager, "addons://list/extension", false, false);

  double_click_addon_element(aManager, "test1@tests.mozilla.org");

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_detail(aManager, "addons://list/extension", true, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("detail-uninstall-btn"),
                                     { }, aManager);

  await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/extension", true, false);

  close_manager(aManager, run_next_test);
});

// Tests that opening the manager opens the last view
add_test(async function() {
  let aManager = await open_manager("addons://list/plugin");
  info("Part 1");
  is_in_list(aManager, "addons://list/plugin", false, false);

  await close_manager(aManager);
  aManager = await open_manager(null);
  info("Part 2");
  is_in_list(aManager, "addons://list/plugin", false, false);

  close_manager(aManager, run_next_test);
});

// Tests that navigating the discovery page works when that was the first view
add_test(async function() {
  let aManager = await open_manager("addons://discover/");
  info("1");
  is_in_discovery(aManager, MAIN_URL, false, false);

  await clickLink(aManager, "link-good");
  info("2");
  is_in_discovery(aManager, SECOND_URL, true, false);

  // Execute go_back only after waitForLoad() has had a chance to setup
  // its listeners.
  executeSoon(go_back);

  await waitForLoad(aManager);
  info("3");
  is_in_discovery(aManager, MAIN_URL, false, true);

  // Execute go_forward only after waitForLoad() has had a chance to setup
  // its listeners.
  executeSoon(go_forward);

  await waitForLoad(aManager);
  is_in_discovery(aManager, SECOND_URL, true, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

  aManager = await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  is_in_discovery(aManager, SECOND_URL, true, true);

  go_back();

  await waitForLoad(aManager);
  is_in_discovery(aManager, MAIN_URL, false, true);

  close_manager(aManager, run_next_test);
});

// Tests that navigating the discovery page works when that was the second view
add_test(async function() {
  let aManager = await open_manager("addons://list/plugin");
  is_in_list(aManager, "addons://list/plugin", false, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-discover"), { }, aManager);

  aManager = await wait_for_view_load(aManager);
  is_in_discovery(aManager, MAIN_URL, true, false);

  await clickLink(aManager, "link-good");
  is_in_discovery(aManager, SECOND_URL, true, false);

  // Execute go_back only after waitForLoad() has had a chance to setup
  // its listeners.
  executeSoon(go_back);

  await waitForLoad(aManager);
  is_in_discovery(aManager, MAIN_URL, true, true);

  // Execute go_forward only after waitForLoad() has had a chance to setup
  // its listeners.
  executeSoon(go_forward);

  await waitForLoad(aManager);
  is_in_discovery(aManager, SECOND_URL, true, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-plugin"), { }, aManager);

  aManager = await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/plugin", true, false);

  go_back();

  aManager = await wait_for_view_load(aManager);
  is_in_discovery(aManager, SECOND_URL, true, true);

  go_back();

  await waitForLoad(aManager);
  is_in_discovery(aManager, MAIN_URL, true, true);

  go_back();

  aManager = await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/plugin", false, true);

  go_forward();

  aManager = await wait_for_view_load(aManager);
  is_in_discovery(aManager, MAIN_URL, true, true);

  // Execute go_forward only after waitForLoad() has had a chance to setup
  // its listeners.
  executeSoon(go_forward);

  await waitForLoad(aManager);
  is_in_discovery(aManager, SECOND_URL, true, true);

  close_manager(aManager, run_next_test);
});

// Tests that refreshing the disicovery pane integrates properly with history
add_test(async function() {
  let aManager = await open_manager("addons://list/plugin");
  is_in_list(aManager, "addons://list/plugin", false, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-discover"), { }, aManager);

  aManager = await wait_for_view_load(aManager);
  is_in_discovery(aManager, MAIN_URL, true, false);

  await clickLink(aManager, "link-good");
  is_in_discovery(aManager, SECOND_URL, true, false);

  EventUtils.synthesizeMouseAtCenter(aManager.document.getElementById("category-discover"), { }, aManager);

  await waitForLoad(aManager);
  is_in_discovery(aManager, MAIN_URL, true, false);

  go_back();

  await waitForLoad(aManager);
  is_in_discovery(aManager, SECOND_URL, true, true);

  go_back();

  await waitForLoad(aManager);
  is_in_discovery(aManager, MAIN_URL, true, true);

  go_back();

  aManager = await wait_for_view_load(aManager);
  is_in_list(aManager, "addons://list/plugin", false, true);

  go_forward();

  aManager = await wait_for_view_load(aManager);
  is_in_discovery(aManager, MAIN_URL, true, true);

  // Execute go_forward only after waitForLoad() has had a chance to setup
  // its listeners.
  executeSoon(go_forward);

  await waitForLoad(aManager);
  is_in_discovery(aManager, SECOND_URL, true, true);

  // Execute go_forward only after waitForLoad() has had a chance to setup
  // its listeners.
  executeSoon(go_forward);

  await waitForLoad(aManager);
  is_in_discovery(aManager, MAIN_URL, true, false);

  close_manager(aManager, run_next_test);
});
