var testURL_01 = chromeRoot + "browser_blank_01.html";
var testURL_02 = chromeRoot + "browser_blank_02.html";

var titleURL = baseURI + "browser_title.sjs?";
var pngURL = "data:image/gif;base64,R0lGODlhCwALAIAAAAAA3pn/ZiH5BAEAAAEALAAAAAALAAsAAAIUhA+hkcuO4lmNVindo7qyrIXiGBYAOw==";

// A queue to order the tests and a handle for each test
var gTests = [];
var gCurrentTest = null;

var back = document.getElementById("tool-back");
var forward = document.getElementById("tool-forward");

function pageLoaded(aURL) {
  return function() {
    let tab = gCurrentTest._currentTab;
    return !tab.isLoading() && tab.browser.currentURI.spec == aURL;
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

function waitForPageShow(aPageURL, aCallback) {
  messageManager.addMessageListener("pageshow", function(aMessage) {
    if (aMessage.target.currentURI.spec == aPageURL) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      setTimeout(function() { aCallback(); }, 0);
    }
  });
};

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
    let urlbarTitle = document.getElementById("urlbar-title");

    // Wait for the awesomebar to load, then do the test
    window.addEventListener("NavigationPanelShown", function() {
      window.removeEventListener("NavigationPanelShown", arguments.callee, false);
      setTimeout(gCurrentTest.onFocusReady, 0);
    }, false);
    EventUtils.synthesizeMouse(urlbarTitle, urlbarTitle.width / 2, urlbarTitle.height / 2, {});
  },

  onFocusReady: function() {
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

    // It looks like there is a race condition somewhere that result having
    // testURL_01 concatenate with testURL_02 as a urlbar value, so to
    // workaround that we're waiting for the readonly state to be fully updated
    function URLIsReadWrite() {
      return BrowserUI._edit.readOnly == false;
    }

    waitFor(function() {
      // Wait for the tab to load, then do the test
      waitFor(gCurrentTest.onPageFinish, pageLoaded(testURL_02));

      setTimeout(function() {
        is(BrowserUI._edit.value, testURL_02, "URL value should be equal to the string sent via synthesizeString");
        EventUtils.synthesizeKey("VK_RETURN", {}, window);
      }, 0);
    }, URLIsReadWrite);
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

    BrowserUI.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});

// Bug 611327 -----------------------------------------------------------------
// Check for urlbar label value
gTests.push({
  desc: "Check for urlbar label value on different cases",
  _currentTab: null,

  run: function() {
    gCurrentTest._currentTab = BrowserUI.newTab(titleURL + "no_title");
    waitForPageShow(titleURL + "no_title", gCurrentTest.onPageLoadWithoutTitle);
  },

  onPageLoadWithoutTitle: function() {
    let urlbarTitle = document.getElementById("urlbar-title");
    is(urlbarTitle.value, titleURL + "no_title", "The title should be equal to the URL");

    BrowserUI.closeTab(gCurrentTest._currentTab);

    gCurrentTest._currentTab = BrowserUI.newTab(titleURL + "english_title");
    waitForPageShow(titleURL + "english_title", gCurrentTest.onPageLoadWithTitle);
  },

  onPageLoadWithTitle: function() {
    let urlbarTitle = document.getElementById("urlbar-title");
    is(urlbarTitle.value, "English Title Page", "The title should be equal to the page title");

    BrowserUI.closeTab(gCurrentTest._currentTab);

    gCurrentTest._currentTab = BrowserUI.newTab(titleURL + "dynamic_title");
    messageManager.addMessageListener("pageshow", function(aMessage) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      gCurrentTest.onBeforePageTitleChanged();
    });

    messageManager.addMessageListener("DOMTitleChanged", function(aMessage) {
      messageManager.removeMessageListener("DOMTitleChanged", arguments.callee);
      urlbarTitle.addEventListener("DOMAttrModified", function(aEvent) {
        if (aEvent.attrName == "value") {
          urlbarTitle.removeEventListener("DOMAttrModified", arguments.callee, false);
          setTimeout(gCurrentTest.onPageTitleChanged, 0);
        }
      }, false);
    });
  },

  onBeforePageTitleChanged: function() {
    let urlbarTitle = document.getElementById("urlbar-title");
    isnot(urlbarTitle.value, "This is not a french title", "The title should not be equal to the new page title yet");
  },

  onPageTitleChanged: function() {
    let urlbarTitle = document.getElementById("urlbar-title");
    is(urlbarTitle.value, "This is not a french title", "The title should be equal to the new page title");

    BrowserUI.closeTab(gCurrentTest._currentTab);

    gCurrentTest._currentTab = BrowserUI.newTab(titleURL + "redirect");
    waitForPageShow(titleURL + "no_title", gCurrentTest.onPageRedirect);
  },

  onPageRedirect: function() {
    let urlbarTitle = document.getElementById("urlbar-title");
    is(urlbarTitle.value, gCurrentTest._currentTab.browser.currentURI.spec, "The title should be equal to the redirected page url");

    BrowserUI.closeTab(gCurrentTest._currentTab);

    gCurrentTest._currentTab = BrowserUI.newTab(titleURL + "location");
    waitForPageShow(titleURL + "no_title", gCurrentTest.onPageLocation);
  },

  onPageLocation: function() {
    let urlbarTitle = document.getElementById("urlbar-title");
    is(urlbarTitle.value, gCurrentTest._currentTab.browser.currentURI.spec, "The title should be equal to the relocate page url");

    BrowserUI.closeTab(gCurrentTest._currentTab);

    // Wait for the awesomebar to load, then do the test
    window.addEventListener("NavigationPanelShown", function() {
      window.removeEventListener("NavigationPanelShown", arguments.callee, false);

      setTimeout(function() {
        EventUtils.synthesizeString(testURL_02, window);
        EventUtils.synthesizeKey("VK_RETURN", {}, window);

        waitForPageShow(testURL_02, gCurrentTest.onUserTypedValue);
      }, 0);

    }, false);

    gCurrentTest._currentTab = BrowserUI.newTab("about:blank");
  },

  onUserTypedValue: function() {
    let urlbarTitle = document.getElementById("urlbar-title");
    is(urlbarTitle.value, "Browser Blank Page 02", "The title should be equal to the typed page url title");

    // about:blank has been closed, so we need to close the last selected one
    BrowserUI.closeTab(Browser.selectedTab);

    // Wait for the awesomebar to load, then do the test
    window.addEventListener("NavigationPanelShown", function() {
      window.removeEventListener("NavigationPanelShown", arguments.callee, false);

      EventUtils.synthesizeString("no_title", window);

      // Wait until the no_title result row is here
      let popup = document.getElementById("popup_autocomplete");
      let result = null;
      function hasResults() {
        result = popup._items.childNodes[0];
        if (result)
          return result.getAttribute("value") == (titleURL + "no_title");

        return false;
      };
      waitFor(function() { EventUtils.synthesizeMouse(result, result.width / 2, result.height / 2, {}); }, hasResults);

      urlbarTitle.addEventListener("DOMAttrModified", function(aEvent) {
        if (aEvent.attrName == "value") {
          urlbarTitle.removeEventListener("DOMAttrModified", arguments.callee, false);
          is(urlbarTitle.value, titleURL + "no_title", "The title should be equal to the url of the clicked row");
        }
      }, false);

      waitForPageShow(titleURL + "no_title", gCurrentTest.onUserSelectValue);
    }, false);

    gCurrentTest._currentTab = BrowserUI.newTab("about:blank");
  },

  onUserSelectValue: function() {
    let urlbarTitle = document.getElementById("urlbar-title");
    is(urlbarTitle.value, Browser.selectedTab.browser.currentURI.spec, "The title should be equal to the clicked page url");

    // about:blank has been closed, so we need to close the last selected one
    BrowserUI.closeTab(Browser.selectedTab);

    //is(urlbarTitle.value, "Browser Blank Page 02", "The title of the second page must be displayed");
    runNextTest();
  }
});

// Case: Check for appearance of the favicon
gTests.push({
  desc: "Check for appearance of the favicon",
  _currentTab: null,

  run: function() {
    gCurrentTest._currentTab = BrowserUI.newTab(testURL_01);
    waitForPageShow(testURL_01, gCurrentTest.onPageReady);
  },

  onPageReady: function() {
    let favicon = document.getElementById("urlbar-favicon");
    is(favicon.src, "", "The default favicon must be loaded");
    BrowserUI.closeTab(gCurrentTest._currentTab);

    gCurrentTest._currentTab = BrowserUI.newTab(testURL_02);
    waitForPageShow(testURL_02, gCurrentTest.onPageFinish);
  },

  onPageFinish: function(){
    let favicon = document.getElementById("urlbar-favicon");
    is(favicon.src, pngURL, "The page favicon must be loaded");
    BrowserUI.closeTab(gCurrentTest._currentTab);
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

    messageManager.addMessageListener("Content:LocationChange", gCurrentTest.onFragmentLoaded);
    Browser.loadURI(testURL_01 + "#fragment");
  },

  onFragmentLoaded: function() {
    messageManager.removeMessageListener("Content:LocationChange", arguments.callee);

    ok(!back.disabled, "Can go back");
    ok(forward.disabled, "Can't go forward");

    messageManager.addMessageListener("Content:LocationChange", gCurrentTest.onBack);
    CommandUpdater.doCommand("cmd_back");
  },

  onBack: function() {
    messageManager.removeMessageListener("Content:LocationChange", arguments.callee);

    ok(back.disabled, "Can't go back");
    ok(!forward.disabled, "Can go forward");

    messageManager.addMessageListener("Content:LocationChange", gCurrentTest.onForward);
    CommandUpdater.doCommand("cmd_forward");
  },

  onForward: function() {
    messageManager.removeMessageListener("Content:LocationChange", arguments.callee);

    ok(!back.disabled, "Can go back");
    ok(forward.disabled, "Can't go forward");

    gCurrentTest.finish();
  },

  finish: function() {
    BrowserUI.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});
