let testURL = chromeRoot + "browser_forms.html";
messageManager.loadFrameScript(chromeRoot + "remote_forms.js", true);

let newTab = null;

function test() {
  // This test is async
  waitForExplicitFinish();

  // Need to wait until the page is loaded
  messageManager.addMessageListener("pageshow", function(aMessage) {
    if (newTab && newTab.browser.currentURI.spec != "about:blank") {
      messageManager.removeMessageListener(aMessage.name, arguments.callee);
      setTimeout(onTabLoaded, 0);
    }
  });

  // Add new tab to hold the <FormAssistant> page
  newTab = Browser.addTab(testURL, true);
}

function onTabLoaded() {
  BrowserUI.closeAutoComplete(true);
  testMouseEvents();
}

function testMouseEvents() {
  // Sending a synthesized event directly on content should not work - we
  // don't want web content to be able to open the form helper without the
  // user consent, so we have to pass through the canvas tile-container
  AsyncTests.waitFor("Test:Click", {}, function(json) {
    is(json.result, false, "Form Assistant should stay closed");
  });

  AsyncTests.waitFor("Test:Focus", { value: "#root" }, function(json) {
    is(json.result, false, "Form Assistant should stay closed");
  });

  AsyncTests.waitFor("Test:FocusRedirect", { value: "*[tabindex='0']" }, function(json) {
    is(json.result, false, "Form Assistant should stay closed");
    testOpenUIWithSyncFocus();
  });
};

function waitForFormAssist(aCallback) {
  messageManager.addMessageListener("FormAssist:Show", function(aMessage) {
    messageManager.removeMessageListener(aMessage.name, arguments.callee);
    setTimeout(function() {
      ok(FormHelperUI._open, "Form Assistant should be open");
      setTimeout(aCallback, 0);
    });
  });
};

function testOpenUIWithSyncFocus() {
  AsyncTests.waitFor("Test:Open", { value: "*[tabindex='0']" }, function(json) {});
  waitForFormAssist(testOpenUI);
};

function testOpenUI() {
  AsyncTests.waitFor("Test:Open", { value: "*[tabindex='0']" }, function(json) {});
  waitForFormAssist(testOpenUIWithFocusRedirect);
};

function testOpenUIWithFocusRedirect() {
  AsyncTests.waitFor("Test:OpenWithFocusRedirect", { value: "*[tabindex='0']" }, function(json) {});
  waitForFormAssist(testShowUIForSelect);
};

function testShowUIForSelect() {
  AsyncTests.waitFor("Test:CanShowUI", { value: "#select"}, function(json) {
    ok(json.result, "canShowUI for select element'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "#select", disabled: true }, function(json) {
    is(json.result, false, "!canShowUI for disabled select element'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "#option"}, function(json) {
    ok(json.result, "canShowUI for option element'");
  });

  AsyncTests.waitFor("Test:CanShowUISelect", { value: "#option", disabled: true }, function(json) {
    is(json.result, false, "!canShowUI for option element with a disabled parent select element'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "#option", disabled: true }, function(json) {
    is(json.result, false, "!canShowUI for disabled option element'");
    testShowUIForElements();
  });
}

function testShowUIForElements() {
  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='1']" }, function(json) {
    ok(json.result, "canShowUI for input type='text'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='1']", disabled: true }, function(json) {
    is(json.result, false, "!canShowUI for disabled input type='text'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='2']" }, function(json) {
    ok(json.result, "canShowUI for input type='password'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='2']", disabled: true }, function(json) {
    is(json.result, false, "!canShowUI for disabled input type='password'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='8']" }, function(json) {
    ok(json.result, "canShowUI for contenteditable div");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='8']", disabled: true }, function(json) {
    is(json.result, false, "!canShowUI for disabled contenteditable div");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='3']" }, function(json) {
    is(json.result, false, "!canShowUI for input type='submit'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='4']" }, function(json) {
    is(json.result, false, "!canShowUI for input type='file'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='5']" }, function(json) {
    is(json.result, false, "!canShowUI for input button type='submit'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='6']" }, function(json) {
    is(json.result, false, "!canShowUI for input div@role='button'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='6']" }, function(json) {
    is(json.result, false, "!canShowUI for input type='image'");
  });

  // Open the Form Helper
  AsyncTests.waitFor("Test:Open", { value: "*[tabindex='1']" }, function(json) {
    ok(json.result, "Form Assistant should be open");
    testTabIndexNavigation();
  });
};

function testTabIndexNavigation() {
  AsyncTests.waitFor("Test:Previous", { value: "*[tabindex='0']" }, function(json) {
    is(json.result, false, "Focus should not have changed");
  });

  AsyncTests.waitFor("Test:Next", { value: "*[tabindex='2']" }, function(json) {
    is(json.result, true, "Focus should be on element with tab-index : 2");
  });

  AsyncTests.waitFor("Test:Previous", { value: "*[tabindex='1']" }, function(json) {
    is(json.result, true, "Focus should be on element with tab-index : 1");
  });

  AsyncTests.waitFor("Test:Next");
  AsyncTests.waitFor("Test:Next");
  AsyncTests.waitFor("Test:Next");
  AsyncTests.waitFor("Test:Next");
  AsyncTests.waitFor("Test:Next");

  AsyncTests.waitFor("Test:Next", { value: "*[tabindex='7']" }, function(json) {
    is(json.result, true, "Focus should be on element with tab-index : 7");
  });

  AsyncTests.waitFor("Test:Next", { value: "*[tabindex='8']" }, function(json) {
    is(json.result, true, "Focus should be on element with tab-index : 8");
  });

  AsyncTests.waitFor("Test:Next", { value: "*[tabindex='0']" }, function(json) {
    is(json.result, true, "Focus should be on element with tab-index : 0");
  });

  let ids = ["next", "select", "dumb", "reset", "checkbox", "radio0", "radio4", "last", "last"];
  for (let i = 0; i < ids.length; i++) {
    let id = ids[i];
    AsyncTests.waitFor("Test:Next", { value: "#" + id }, function(json) {
      is(json.result, true, "Focus should be on element with #id: " + id + "");
    });
  };

  FormHelperUI.hide();
  let container = document.getElementById("content-navigator");
  is(container.hidden, true, "Form Assistant should be close");

  AsyncTests.waitFor("Test:Open", { value: "*[tabindex='0']" }, function(json) {
    ok(FormHelperUI._open, "Form Assistant should be open");
    testFocusChanges();
  });
};

function testFocusChanges() {
  AsyncTests.waitFor("Test:Focus", { value: "*[tabindex='1']" }, function(json) {
    ok(json.result, "Form Assistant should be open");
  });

  AsyncTests.waitFor("Test:Focus", { value: "#select" }, function(json) {
    ok(json.result, "Form Assistant should stay open");
  });

  AsyncTests.waitFor("Test:Focus", { value: "*[type='hidden']" }, function(json) {
    ok(json.result, "Form Assistant should stay open");
    loadNestedIFrames();
  });
}

function loadNestedIFrames() {
  AsyncTests.waitFor("Test:Iframe", { }, function(json) {
    is(json.result, true, "Iframe should have loaded");
    navigateIntoNestedIFrames();
  });
}

function navigateIntoNestedIFrames() {
  AsyncTests.waitFor("Test:IframeOpen", { }, function(json) {
    is(json.result, true, "Form Assistant should have been opened");
  });

  AsyncTests.waitFor("Test:IframePrevious", { value: 0 }, function(json) {
    is(json.result, true, "Focus should not have move");
  });

  AsyncTests.waitFor("Test:IframeNext", { value: 1 }, function(json) {
    is(json.result, true, "Focus should have move");
  });

  AsyncTests.waitFor("Test:IframeNext", { value: 1 }, function(json) {
    is(json.result, true, "Focus should not have move");

    // Close the form assistant
    FormHelperUI.hide();

    // Close our tab when finished
    Browser.closeTab(newTab);

    // We must finalize the tests
    finish();
  });
};

