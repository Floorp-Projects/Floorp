let testURL = "chrome://mochikit/content/browser/mobile/chrome/browser_select.html";
let new_tab = null;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();

  // Add new tab to hold the <select> page
  new_tab = Browser.addTab(testURL, true);
  ok(new_tab, "Tab Opened");

  // Need to wait until the page is loaded
  messageManager.addMessageListener("pageshow",
  function(aMessage) {
    if (new_tab.browser.currentURI.spec != "about:blank") {
      messageManager.removeMessageListener(aMessage.name, arguments.callee);
      onPageReady();
    }
  });
}

function onPageReady() {
  let combo = new_tab.browser.contentDocument.getElementById("combobox");
  isnot(combo, null, "Get the select from web content");

  // Sending a synthesized event to the combo is not working
  let rect = Browser.browserViewToClientRect(Rect.fromRect(combo.getBoundingClientRect()));
  ContentTouchHandler.tapSingle(rect.left + 1, rect.top + 1);

  waitFor(onUIReady, function() { return document.getElementById("select-container").hidden == false; });
}

function onUIReady() {
  let selectui = document.getElementById("select-container");
  is(selectui.hidden, false, "Select UI should be open");

  let doneButton = document.getElementById("select-buttons-done");
  doneButton.click();

  // Close our tab when finished
  Browser.closeTab(new_tab);

  // We must finialize the tests
  finish();
}
