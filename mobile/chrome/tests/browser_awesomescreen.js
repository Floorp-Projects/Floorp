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
    // Close the awesome panel just in case
    AwesomeScreen.activePanel = null;
    finish();
  }
}

function waitForNavigationPanel(aCallback, aWaitForHide) {
  let evt = aWaitForHide ? "NavigationPanelHidden" : "NavigationPanelShown";
  info("waitFor " + evt + "(" + Components.stack.caller + ")");
  window.addEventListener(evt, function(aEvent) {
    info("receive " + evt);
    window.removeEventListener(aEvent.type, arguments.callee, false);
    setTimeout(aCallback, 0);
  }, false);
}

//------------------------------------------------------------------------------
// Case: Test awesome bar collapsed state
gTests.push({
  desc: "Test awesome bar collapsed state",

  run: function() {
    waitForNavigationPanel(gCurrentTest.onPopupShown);
    AllPagesList.doCommand();
  },

  onPopupShown: function() {
    is(AwesomeScreen.activePanel, AllPagesList, "AllPagesList should be visible");
    ok(!BrowserUI._edit.collapsed, "The urlbar edit element is visible");
    ok(BrowserUI._title.collapsed, "The urlbar title element is not visible");

    waitForNavigationPanel(gCurrentTest.onPopupHidden, true);
    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  },

  onPopupHidden: function() {
    is(AwesomeScreen.activePanel, null, "AllPagesList should be dismissed");
    ok(BrowserUI._edit.collapsed, "The urlbar edit element is not visible");
    ok(!BrowserUI._title.collapsed, "The urlbar title element is visible");

    runNextTest();
  }
});


//------------------------------------------------------------------------------
// Case: Test typing a character should dismiss the awesome header
gTests.push({
  desc: "Test typing a character should dismiss the awesome header",

  run: function() {
    waitForNavigationPanel(gCurrentTest.onPopupReady);
    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    is(AwesomeScreen.activePanel == AllPagesList, true, "AllPagesList should be visible");

    let awesomeHeader = document.getElementById("awesome-header");
    is(awesomeHeader.hidden, false, "Awesome header should be visible");

    BrowserUI._edit.addEventListener("onsearchbegin", function(aEvent) {
      if (BrowserUI._edit.value == "")
        return;

      BrowserUI._edit.removeEventListener(aEvent.type, arguments.callee, true);
      let awesomeHeader = document.getElementById("awesome-header");
      is(awesomeHeader.hidden, true, "Awesome header should be hidden");
      gCurrentTest.onKeyPress();
    }, true);
    EventUtils.synthesizeKey("A", {}, window);
  },

  onKeyPress: function(aKey, aHidden) {
    waitForNavigationPanel(function() {
      let awesomeHeader = document.getElementById("awesome-header");
      is(awesomeHeader.hidden, false, "Awesome header should be visible");
      runNextTest();
    }, true);

    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  }
});

//------------------------------------------------------------------------------
// Case: Test typing a character should open the awesome bar
gTests.push({
  desc: "Test typing a character should open the All Pages List",

  run: function() {
    waitForNavigationPanel(gCurrentTest.onPopupReady);
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
    is(AwesomeScreen.activePanel == AllPagesList, true, "AllPagesList should be opened on a keydown");
    is(BrowserUI._edit.readOnly, false, "urlbar should not be readonly after an input");

    waitForNavigationPanel(gCurrentTest.onPopupHidden, true);
    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  },

  onPopupHidden: function() {
    is(AwesomeScreen.activePanel == null, true, "VK_ESCAPE should have dismissed the awesome panel");
    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Case: Test opening the awesome panel and checking the urlbar readonly state
gTests.push({
  desc: "Test opening the awesome panel and checking the urlbar readonly state",

  run: function() {
    is(BrowserUI._edit.readOnly, true, "urlbar input textbox should be readonly");

    waitForNavigationPanel(gCurrentTest.onPopupReady);
    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    is(Elements.urlbarState.getAttribute("mode"), "edit", "bcast_urlbarState mode attribute should be equal to 'edit'");

    let edit = BrowserUI._edit;
    is(edit.readOnly, BrowserUI._isKeyboardFullscreen(), "urlbar input textbox is readonly if keyboard is fullscreen, editable otherwise");

    let urlString = BrowserUI.getDisplayURI(Browser.selectedBrowser);
    if (Util.isURLEmpty(urlString))
      urlString = "";

    let firstPanel = true;
    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      is(AwesomeScreen.activePanel, aPanel, "The panel " + aPanel.panel.id + " should be selected");
      if (firstPanel) {
        // First panel will have selected text, if we are in portrait
        is(edit.readOnly, BrowserUI._isKeyboardFullscreen(), "urlbar input textbox is readonly if keyboard is fullscreen, editable otherwise");
      } else {
        is(edit.readOnly, true, "urlbar input textbox be readonly if not the first panel");
      }
      edit.click();
      is(edit.readOnly, false, "urlbar input textbox should not be readonly after a click, in both landscape and portrait");
      is(edit.value, urlString, "urlbar value should be equal to the page uri");

      firstPanel = false;
    });

    setTimeout(function() {
      AwesomeScreen.activePanel = null;
      runNextTest();
    }, 0);
  }
});

//------------------------------------------------------------------------------
// Case: Test opening the awesome panel and checking the urlbar selection
gTests.push({
  desc: "Test opening the awesome panel and checking the urlbar selection",

  run: function() {
    BrowserUI.closeAutoComplete(true);
    this.currentTab = BrowserUI.newTab(testURL_01);

    // Need to wait until the page is loaded
    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      if (gCurrentTest.currentTab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        setTimeout(gCurrentTest.onPageReady, 0);
      }
    });
  },

  onPageReady: function() {
    waitForNavigationPanel(gCurrentTest.onPopupReady);

    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    let edit = BrowserUI._edit;

    let firstPanel = true;
    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      if (firstPanel && !BrowserUI._isKeyboardFullscreen()) {
        // First panel will have selected text, if we are in portrait
        ok(edit.selectionStart == 0 && edit.selectionEnd == edit.textLength, "[case 1] urlbar text should be selected on a simple show");
        edit.click();
        // The click is not sync enough for this to work
        todo(edit.selectionStart == edit.selectionEnd, "[case 1] urlbar text should not be selected on a click");
      } else {
        ok(edit.selectionStart == edit.selectionEnd, "[case 2] urlbar text should not be selected on a simple show");
        edit.click();
        ok(edit.selectionStart == 0 && edit.selectionEnd == edit.textLength, "[case 2] urlbar text should be selected on a click");
      }
      firstPanel = false;
    });

    // We are disabling it early, otherwise calling edit.click(); quickly made input.js though this is a double click (sigh)
    let oldDoubleClickSelectsAll = Services.prefs.getBoolPref("browser.urlbar.doubleClickSelectsAll");
    Services.prefs.setBoolPref("browser.urlbar.doubleClickSelectsAll", false);

    let oldClickSelectsAll = edit.clickSelectsAll;
    edit.clickSelectsAll = false;
    firstPanel = true;
    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      if (firstPanel && !BrowserUI._isKeyboardFullscreen()) {
        // First panel will have selected text, if we are in portrait
        ok(edit.selectionStart == 0 && edit.selectionEnd == edit.textLength, "[case 1] urlbar text should be selected on a simple show");
        edit.click();
        // The click is not sync enough for this to work
        todo(edit.selectionStart == edit.selectionEnd, "[case 1] urlbar text should not be selected on a click");
      } else {
        ok(edit.selectionStart == edit.selectionEnd, "[case 2] urlbar text should not be selected on a simple show");
        edit.click();
        ok(edit.selectionStart == edit.selectionEnd, "[case 2] urlbar text should not be selected on a click");
      }

      firstPanel = false;
    });

    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      ok(edit.selectionStart == edit.selectionEnd, "urlbar text should not be selected on a simple show");
      edit.click();
      edit.click();
      ok(edit.selectionStart == edit.selectionEnd, "urlbar text should not be selected on a double click");
    });

    Services.prefs.setBoolPref("browser.urlbar.doubleClickSelectsAll", oldDoubleClickSelectsAll);

    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      ok(edit.selectionStart == edit.selectionEnd, "urlbar text should not be selected on a simple show");
      edit.click();
      edit.click();
      ok(edit.selectionStart == 0 && edit.selectionEnd == edit.textLength, "urlbar text should be selected on a double click");
    });

    edit.clickSelectsAll = oldClickSelectsAll;

    AwesomeScreen.activePanel = null;

    // Ensure the tab is well closed before doing the rest of the code, otherwise
    // this cause some bugs with the composition events
    let tabCount = Browser.tabs.length;
    Browser.closeTab(gCurrentTest.currentTab, { forceClose: true });
    waitFor(runNextTest, function() Browser.tabs.length == tabCount - 1);
  }
});

// Case: Test context clicks on awesome panel
gTests.push({
  desc: "Test context clicks on awesome panel",

  _panelIndex : 0,
  _contextOpts : [
    ["link-openable", "link-shareable"],
    ["link-openable", "link-shareable"],
    ["edit-bookmark", "link-shareable", "link-openable"],
  ],

  clearContextTypes: function clearContextTypes() {
    if (ContextHelper.popupState)
      ContextHelper.hide();
  },

  checkContextTypes: function checkContextTypes(aTypes) {
    let commandlist = document.getElementById("context-commands");
  
    for (let i=0; i<commandlist.childNodes.length; i++) {
      let command = commandlist.childNodes[i];
      if (aTypes.indexOf(command.getAttribute("type")) > -1) {
        // command should be visible
        if(command.hidden == true)
          return false;
      } else {
        if(command.hidden == false)
          return false;
      }
    }
    return true;
  },

  run: function() {
    waitForNavigationPanel(gCurrentTest.onPopupReady);
    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    let self = this;
    if(self._panelIndex < Panels.length) {
      let panel = Panels[self._panelIndex];
      panel.doCommand();

      self.clearContextTypes();      

      EventUtils.synthesizeMouse(panel.panel, panel.panel.width / 2, panel.panel.height / 2, { type: "mousedown" });
      setTimeout(function() {
        EventUtils.synthesizeMouse(panel.panel, panel.panel.width / 2, panel.panel.height / 2, { type: "mouseup" });
        ok(self.checkContextTypes(self._contextOpts[self._panelIndex]), "Correct context menu shown for panel");
        self.clearContextTypes();
  
        self._panelIndex++;
        self.onPopupReady();
      }, 500);
    } else {
      AwesomeScreen.activePanel = null;
      runNextTest();
    }
  }
});


// Case: Test compositionevent
gTests.push({
  desc: "Test sending composition events",
  _textValue: null,
  get popup() {
    delete this.popup;
    return this.popup = document.getElementById("popup_autocomplete");
  },

  get popupHeader() {
    delete this.popupHeader;
    return this.popupHeader = document.getElementById("awesome-header");
  },

  get inputField() {
    delete this.inputField;
    return this.inputField = document.getElementById("urlbar-edit");
  },

  run: function() {
    // Saving value to compare the result before and after the composition event
    gCurrentTest._textValue = gCurrentTest.inputField.value;

    window.addEventListener("popupshown", function() {
      window.removeEventListener("popupshown", arguments.callee, false);
      if (BrowserUI._isKeyboardFullscreen())
        gCurrentTest.inputField.readOnly = false;
      setTimeout(gCurrentTest.onPopupReady, 0);
    }, false);
    AllPagesList.doCommand();
  },

  _checkState: function() {
    ok(gCurrentTest.popup._popupOpen, "AutoComplete popup should be opened");
    is(gCurrentTest.popupHeader.hidden, false, "AutoComplete popup header should be visible");
    is(gCurrentTest.inputField.value, gCurrentTest._textValue, "Value should not have changed");
  },

  onPopupReady: function() {
    gCurrentTest._checkState();

    window.addEventListener("compositionstart", function() {
      window.removeEventListener("compositionstart", arguments.callee, false);
      setTimeout(gCurrentTest.onCompositionStart, 0)
    }, false);
    Browser.windowUtils.sendCompositionEvent("compositionstart");
  },

  onCompositionStart: function() {
    gCurrentTest._checkState();

    window.addEventListener("compositionend", function() {
      window.removeEventListener("compositionend", arguments.callee, false);
      setTimeout(gCurrentTest.onCompositionEnd, 0)
    }, false);
    Browser.windowUtils.sendCompositionEvent("compositionend");
  },

  onCompositionEnd: function() {
    /* TODO: This is currently failing (bug 642771)
    gCurrentTest._checkState();

    let isHiddenHeader = function() {
      return gCurrentTest.popupHeader.hidden;
    }

    // Wait to be sure there the header won't dissapear
    // XXX this sucks because it means we'll be stuck 500ms if the test succeed
    // but I don't have a better idea about how to do it for now since we don't
    // that to happen!

    waitForAndContinue(function() {
      gCurrentTest._checkState();
      runNextTest();
    }, isHiddenHeader, Date.now() + 500);
    */
    runNextTest();
  }
});

