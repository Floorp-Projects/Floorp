var testURL_01 = chromeRoot + "browser_blank_01.html";
var testURL_02 = chromeRoot + "browser_blank_02.html";
var testURL_03 = chromeRoot + "browser_english_title.html";
var testURL_04 = chromeRoot + "browser_no_title.html";
var pngURL = "data:image/gif;base64,R0lGODlhCwALAIAAAAAA3pn/ZiH5BAEAAAEALAAAAAALAAsAAAIUhA+hkcuO4lmNVindo7qyrIXiGBYAOw==";

// A queue to order the tests and a handle for each test
var gTests = [];
var gCurrentTest = null;

var back = document.getElementById("tool-back");
var forward = document.getElementById("tool-forward");

function pageLoaded(url) {
  return function() {
    let tab = gCurrentTest._currentTab;
    return !tab.isLoading() && tab.browser.currentURI.spec == url;
  }
}

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();
  // Start the tests
  runNextTest();
}

//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    info(gCurrentTest.desc);
    gCurrentTest.run();
  }
  else {
    // Cleanup. All tests are completed at this point
    try {
      // Add any cleanup code here
    }
    finally {
      // We must finialize the tests
      finish();
    }
  }
}

//------------------------------------------------------------------------------
// Case: Loading a page into the URLBar with VK_RETURN
gTests.push({
  desc: "Loading a page into the URLBar with VK_RETURN",
  _currentTab: null,

  run: function() {
    gCurrentTest._currentTab = BrowserUI.newTab(testURL_01);

    // Wait for the tab to load, then do the test
    waitFor(gCurrentTest.onPageReady, pageLoaded(testURL_01));
  },

  onPageReady: function() {
    // Test the mode
    let urlIcons = document.getElementById("urlbar-icons");
    is(urlIcons.getAttribute("mode"), "view", "URL Mode is set to 'view'");

    // Test back button state
    is(back.disabled, !gCurrentTest._currentTab.browser.canGoBack, "Back button check");

    // Test forward button state
    is(forward.disabled, !gCurrentTest._currentTab.browser.canGoForward, "Forward button check");

    // Focus the url edit
    let urlbarEdit = document.getElementById("urlbar-edit");

    // Wait for the awesomebar to load, then do the test
    window.addEventListener("NavigationPanelShown", gCurrentTest.onFocusReady, false);
    EventUtils.synthesizeMouse(urlbarEdit, urlbarEdit.width / 2, urlbarEdit.height / 2, {});
  },

  onFocusReady: function() {
    window.removeEventListener("NavigationPanelShown", gCurrentTest.onFocusReady, false);

    // Test mode
    let urlIcons = document.getElementById("urlbar-icons");
    is(urlIcons.getAttribute("mode"), "edit", "URL Mode is set to 'edit'");

    // Test back button state
    is(back.disabled, !gCurrentTest._currentTab.browser.canGoBack, "Back button check");

    // Test forward button state
    is(forward.disabled, !gCurrentTest._currentTab.browser.canGoForward, "Forward button check");

    // Check button states (url edit is focused)
    let search = document.getElementById("tool-search");
    let searchStyle = window.getComputedStyle(search, null);
    is(searchStyle.visibility, "visible", "SEARCH is visible");

    let stop = document.getElementById("tool-stop");
    let stopStyle = window.getComputedStyle(stop, null);
    is(stopStyle.visibility, "collapse", "STOP is hidden");

    let reload = document.getElementById("tool-reload");
    let reloadStyle = window.getComputedStyle(reload, null);
    is(reloadStyle.visibility, "collapse", "RELOAD is hidden");

    // Send the string and press return
    EventUtils.synthesizeString(testURL_02, window);
    EventUtils.synthesizeKey("VK_RETURN", {}, window);

    // Wait for the tab to load, then do the test
    waitFor(gCurrentTest.onPageFinish, pageLoaded(testURL_02));
  },

  onPageFinish: function() {
    let urlIcons = document.getElementById("urlbar-icons");
    is(urlIcons.getAttribute("mode"), "view", "URL Mode is set to 'view'");

    // Check button states (url edit is not focused)
    let search = document.getElementById("tool-search");
    let searchStyle = window.getComputedStyle(search, null);
    is(searchStyle.visibility, "collapse", "SEARCH is hidden");

    let stop = document.getElementById("tool-stop");
    let stopStyle = window.getComputedStyle(stop, null);
    is(stopStyle.visibility, "collapse", "STOP is hidden");

    let reload = document.getElementById("tool-reload");
    let reloadStyle = window.getComputedStyle(reload, null);
    is(reloadStyle.visibility, "visible", "RELOAD is visible");

    let uri = gCurrentTest._currentTab.browser.currentURI.spec;
    is(uri, testURL_02, "URL Matches newly created Tab");

    // Go back in session
    gCurrentTest._currentTab.browser.goBack();

    // Wait for the tab to load, then do the test
    waitFor(gCurrentTest.onPageBack, pageLoaded(testURL_01));
  },

  onPageBack: function() {
    // Test back button state
    is(back.disabled, !gCurrentTest._currentTab.browser.canGoBack, "Back button check");

    // Test forward button state
    is(forward.disabled, !gCurrentTest._currentTab.browser.canGoForward, "Forward button check");

    Browser.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Bug 570706 - --browser-chrome Mochitests on Fennec [post navigation]
// Check for text in the url bar for no title, with title and title change after pageload
gTests.push({
  desc: "Check for text in the url bar for no title, with title and title change after pageload",
  _currentTab: null,

  run: function() {
    gCurrentTest._currentTab = BrowserUI.newTab(testURL_03);

    // Wait for the tab to load, then do the test
    messageManager.addMessageListener("pageshow", function() {
    if (gCurrentTest._currentTab.browser.currentURI.spec == testURL_03) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      gCurrentTest.onPageReady();
    }});
  },

  onPageReady: function() {
    let urlbarEdit = document.getElementById("urlbar-edit");
    is(urlbarEdit.value, "English Title Page", "The title must be displayed in urlbar");
    Browser.closeTab(gCurrentTest._currentTab);
    gCurrentTest._currentTab = BrowserUI.newTab(testURL_04);

    messageManager.addMessageListener("pageshow", function() {
    if (gCurrentTest._currentTab.browser.currentURI.spec == testURL_04) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      gCurrentTest.onPageReady2();
    }});
  },

  onPageReady2: function(){
    let urlbarEdit = document.getElementById("urlbar-edit");
    is(urlbarEdit.value, testURL_04, "The url for no title must be displayed in urlbar");
    Browser.closeTab(gCurrentTest._currentTab);

    // Check whether title appears after a pageload
    gCurrentTest._currentTab = BrowserUI.newTab(testURL_01);
    messageManager.addMessageListener("pageshow", function() {
    if (gCurrentTest._currentTab.browser.currentURI.spec == testURL_01) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      gCurrentTest.onPageReady3();
    }});
  },

  onPageReady3: function(){
    let urlbarEdit = document.getElementById("urlbar-edit");
    is(urlbarEdit.value, "Browser Blank Page 01", "The title of the first page must be displayed");

    // Wait for the awesomebar to load, then do the test
    window.addEventListener("NavigationPanelShown", gCurrentTest.onFocusReady, false);
    EventUtils.synthesizeMouse(urlbarEdit, urlbarEdit.width / 2, urlbarEdit.height / 2, {});
  },

  onFocusReady: function() {
    window.removeEventListener("NavigationPanelShown", gCurrentTest.onFocusReady, false);
    EventUtils.synthesizeString(testURL_02, window);
    EventUtils.synthesizeKey("VK_RETURN", {}, window)

    messageManager.addMessageListener("pageshow", function() {
    if (gCurrentTest._currentTab.browser.currentURI.spec == testURL_02) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      gCurrentTest.onPageFinish();
    }});
  },

  onPageFinish: function() {
    let urlbarEdit = document.getElementById("urlbar-edit");
    is(urlbarEdit.value, "Browser Blank Page 02", "The title of the second page must be displayed");
    Browser.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});

// Case: Check for appearance of the favicon
gTests.push({
  desc: "Check for appearance of the favicon",
  _currentTab: null,

  run: function() {
    gCurrentTest._currentTab = BrowserUI.newTab(testURL_04);
    messageManager.addMessageListener("pageshow", function() {

    if (gCurrentTest._currentTab.browser.currentURI.spec == testURL_04) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      gCurrentTest.onPageReady();
    }});
  },

  onPageReady: function() {
    let favicon = document.getElementById("urlbar-favicon");
    is(favicon.src, "", "The default favicon must be loaded");
    Browser.closeTab(gCurrentTest._currentTab);

    gCurrentTest._currentTab = BrowserUI.newTab(testURL_03);
    messageManager.addMessageListener("pageshow", function() {
    if (gCurrentTest._currentTab.browser.currentURI.spec == testURL_03) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      gCurrentTest.onPageFinish();
    }});
  },

  onPageFinish: function(){
    let favicon = document.getElementById("urlbar-favicon");
    is(favicon.src, pngURL, "The page favicon must be loaded");
    Browser.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});

// Bug 600707 - Back and forward buttons are updated when navigating within a page
//
// These tests use setTimeout instead of waitFor or addEventListener, because
// in-page navigation does not fire any loading events or progress
// notifications, and happens more or less instantly.
gTests.push({
  desc: "Navigating within a page via URI fragments",
  _currentTab: null,

  run: function() {
    gCurrentTest._currentTab = BrowserUI.newTab(testURL_01);
    waitFor(gCurrentTest.onPageReady, pageLoaded(testURL_01));
  },

  onPageReady: function() {
    ok(back.disabled, "Can't go back");
    ok(forward.disabled, "Can't go forward");

    messageManager.addMessageListener("WebProgress:LocationChange", gCurrentTest.onFragmentLoaded);
    Browser.loadURI(testURL_01 + "#fragment");
  },

  onFragmentLoaded: function() {
    messageManager.removeMessageListener("WebProgress:LocationChange", arguments.callee);

    ok(!back.disabled, "Can go back");
    ok(forward.disabled, "Can't go forward");

    messageManager.addMessageListener("WebProgress:LocationChange", gCurrentTest.onBack);
    CommandUpdater.doCommand("cmd_back");
  },

  onBack: function() {
    messageManager.removeMessageListener("WebProgress:LocationChange", arguments.callee);

    ok(back.disabled, "Can't go back");
    ok(!forward.disabled, "Can go forward");

    messageManager.addMessageListener("WebProgress:LocationChange", gCurrentTest.onForward);
    CommandUpdater.doCommand("cmd_forward");
  },

  onForward: function() {
    messageManager.removeMessageListener("WebProgress:LocationChange", arguments.callee);

    ok(!back.disabled, "Can go back");
    ok(forward.disabled, "Can't go forward");

    gCurrentTest.finish();
  },

  finish: function() {
    Browser.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});
