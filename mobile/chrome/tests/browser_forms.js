let testURL = chromeRoot + "browser_forms.html";
messageManager.loadFrameScript(chromeRoot + "remote_forms.js", true);

let newTab = null;

function test() {
  // This test is async
  waitForExplicitFinish();

  // Need to wait until the page is loaded
  messageManager.addMessageListener("pageshow", function(aMessage) {
    if (newTab.browser.currentURI.spec != "about:blank") {
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
  // user consent, so we have to pass throught the canvas tile-container
/*
  AsyncTests.waitFor("Test:Click", {}, function(json) {
    is(json.result, false, "Form Assistant should stay closed");
  });
*/
  AsyncTests.waitFor("Test:Open", { value: "*[tabindex='0']" }, function(json) {
    ok(FormHelperUI._open, "Form Assistant should be open");
    testShowUIForElements();
  });
};

function testShowUIForElements() {
  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='1']" }, function(json) {
    ok(json.result, "canShowUI for input type='text'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='2']" }, function(json) {
    ok(json.result, "canShowUI for input type='password'");
  });

  AsyncTests.waitFor("Test:CanShowUI", { value: "*[tabindex='8']" }, function(json) {
    ok(json.result, "canShowUI for contenteditable div");
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
    AsyncTests.waitFor("Test:Next", { value: "*[id='" + id + "']" }, function(json) {
      is(json.result, true, "Focus should be on element with #id: " + id + "");
    });
  };

  FormHelperUI.hide();
  let container = document.getElementById("content-navigator");
  is(container.hidden, true, "Form Assistant should be close");


  loadNestedIFrames();
};

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

