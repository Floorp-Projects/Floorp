/*
 * Bug 436069 - Fennec browser-chrome tests to verify correct navigation into the
 *              differents part of the awesome panel
 */

let testURL_01 = chromeRoot + "browser_blank_01.html";

let gTests = [];
let gCurrentTest = null;
let Panels = [AllPagesList, HistoryList, BookmarkList];

function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  // Start the tests
  setTimeout(runNextTest, 200);
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
    BrowserUI.closeAutoComplete();
    finish();
  }
}

//------------------------------------------------------------------------------
// Case: Test typing a character should dismiss the awesome header
gTests.push({
  desc: "Test typing a character should dismiss the awesome header",

  run: function() {
    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, true);
      gCurrentTest.onPopupReady();
    }, true);

    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    is(BrowserUI.activePanel == AllPagesList, true, "AllPagesList should be visible");

    let awesomeHeader = document.getElementById("awesome-header");
    is(awesomeHeader.hidden, false, "Awesome header should be visible");

    BrowserUI._edit.addEventListener("onsearchbegin", function(aEvent) {
      BrowserUI._edit.removeEventListener(aEvent.type, arguments.callee, true);
      let awesomeHeader = document.getElementById("awesome-header");
      is(awesomeHeader.hidden, true, "Awesome header should be hidden");
      gCurrentTest.onKeyPress();
    }, true);
    EventUtils.synthesizeKey("A", {}, window);
  },

  onKeyPress: function(aKey, aHidden) {
    window.addEventListener("NavigationPanelHidden", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);
      let awesomeHeader = document.getElementById("awesome-header");
      is(awesomeHeader.hidden, false, "Awesome header should be visible");
      runNextTest();
    }, false);

    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  }
});

//------------------------------------------------------------------------------
// Case: Test typing a character should open the awesome bar
gTests.push({
  desc: "Test typing a character should open the All Pages List",

  run: function() {
    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, true);
      gCurrentTest.onPopupReady();
    }, true);
    BookmarkList.doCommand();
  },

  onPopupReady: function() {
    BrowserUI._edit.addEventListener("onsearchbegin", function(aEvent) {
      BrowserUI._edit.removeEventListener(aEvent.type, arguments.callee, false);
      gCurrentTest.onSearchBegin();
    }, false);
    EventUtils.synthesizeKey("I", {}, window);
  },

  onSearchBegin: function() {
    let awesomeHeader = document.getElementById("awesome-header");
    is(awesomeHeader.hidden, true, "Awesome header should be hidden");
    is(BrowserUI.activePanel == AllPagesList, true, "AllPagesList should be opened on a keydown");
    is(BrowserUI._edit.readOnly, false, "urlbar should not be readonly after an input");

    window.addEventListener("NavigationPanelHidden", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, true);
      gCurrentTest.onPopupHidden();
    }, true);

    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  },

  onPopupHidden: function() {
    is(BrowserUI.activePanel == null, true, "VK_ESCAPE should have dismissed the awesome panel");
    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Case: Test opening the awesome panel and checking the urlbar readonly state
gTests.push({
  desc: "Test opening the awesome panel and checking the urlbar readonly state",

  run: function() {
    is(BrowserUI._edit.readOnly, true, "urlbar input textbox should be readonly");

    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, true);
      gCurrentTest.onPopupReady();
    }, true);

    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    is(Elements.urlbarState.getAttribute("mode"), "edit", "bcast_urlbarState mode attribute should be equal to 'edit'");

    let edit = BrowserUI._edit;
    is(edit.readOnly, true, "urlbar input textbox be readonly once it is open in landscape");

    let urlString = BrowserUI.getDisplayURI(Browser.selectedBrowser);
    if (Util.isURLEmpty(urlString))
      urlString = "";

    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      is(BrowserUI.activePanel, aPanel, "The panel " + aPanel.panel.id + " should be selected");
      is(edit.readOnly, true, "urlbar input textbox be readonly once it is open in landscape");
      edit.click();
      is(edit.readOnly, false, "urlbar input textbox should not be readonly once it is open in landscape and click again");

      is(edit.value, urlString, "urlbar value should be equal to the page uri");
    });

    setTimeout(function() {
      BrowserUI.activePanel = null;
      runNextTest();
    }, 0);
  }
});

//------------------------------------------------------------------------------
// Case: Test opening the awesome panel and checking the urlbar selection
gTests.push({
  desc: "Test opening the awesome panel and checking the urlbar selection",

  run: function() {
    this._currentTab = BrowserUI.newTab(testURL_01);

    // Need to wait until the page is loaded
    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      if (gCurrentTest._currentTab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        gCurrentTest.onPageReady();
      }
    });
  },

  onPageReady: function() {
    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, true);
      gCurrentTest.onPopupReady();
    }, true);

    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    let edit = BrowserUI._edit;

    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      // XXX for some reason the selectionStart == 0 && selectionEnd = edit.textLength
      // even if visually there is no selection at all
      todo_is(edit.selectionStart == edit.textLenght && edit.selectionEnd == edit.textLength, true, "urlbar text should not be selected on a simple show");

      edit.click();
      is(edit.selectionStart == 0 && edit.selectionEnd == edit.textLength, true, "urlbar text should be selected on a click");

    });

    let oldClickSelectsAll = edit.clickSelectsAll;
    edit.clickSelectsAll = false;
    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      // XXX for some reason the selectionStart == 0 && selectionEnd = edit.textLength
      // even if visually there is no selection at all
      todo_is(edit.selectionStart == edit.textLenght && edit.selectionEnd == edit.textLength, true, "urlbar text should not be selected on a simple show");
      edit.click();
      is(edit.selectionStart == 0 && edit.selectionEnd == edit.textLength, true, "urlbar text should be selected on a click");
    });
    edit.clickSelectsAll = oldClickSelectsAll;

    BrowserUI.closeTab(this._currentTab);

    Util.executeSoon(function() {
      BrowserUI.activePanel = null;
      runNextTest();
    });
  }
});

