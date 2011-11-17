let testURL = chromeRoot + "browser_select.html";
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

  new_tab.browser.contentWindow.addEventListener('mousedown', function(e) {
    ok(false, e.type + ' should not have been fired');
  }, true);
  new_tab.browser.contentWindow.addEventListener('mouseup', function(e) {
    ok(false, e.type + ' should not have been fired');
  }, true);
  new_tab.browser.contentWindow.addEventListener('click', function(e) {
    ok(false, e.type + ' should not have been fired');
  }, true);

  let rect = browserViewToClientRect(Rect.fromRect(combo.getBoundingClientRect()));
  ContentTouchHandler.tapDown(rect.left + 1, rect.top + 1);
  ContentTouchHandler.tapSingle(rect.left + 1, rect.top + 1);
  ContentTouchHandler.tapUp(rect.left + 1, rect.top + 1);

  waitFor(closeSelect, function() { return document.getElementById("select-container").hidden == false; });
}

function closeSelect() {
  let selectui = document.getElementById("select-container");
  is(selectui.hidden, false, "Select UI should be open");

  EventUtils.sendKey("ESCAPE", window);

  waitFor(tapOnMultiSelect, function() { return document.getElementById("select-container").hidden == true; });
}

function tapOnMultiSelect() {
  let selectui = document.getElementById("select-container");
  is(selectui.hidden, true, "Select UI should be hidden");

  let option = new_tab.browser.contentDocument.getElementById("listbox-multiselect");
  let rect = browserViewToClientRect(Rect.fromRect(option.getBoundingClientRect()));
  ContentTouchHandler.tapDown(rect.left + 1, rect.top + 1);
  ContentTouchHandler.tapSingle(rect.left + 1, rect.top + 1);
  ContentTouchHandler.tapUp(rect.left + 1, rect.top + 1);

  waitFor(onUIReady, function() { return document.getElementById("select-container").hidden == false; });
}

function onUIReady() {
  let selectui = document.getElementById("select-container");
  is(selectui.hidden, false, "Select UI should be open");
  is(SelectHelperUI._selectedIndexes, 7, "Select UI should have the 8th option selected:" + SelectHelperUI._selectedIndexes);

  EventUtils.sendKey("ESCAPE", window);

  // Close our tab when finished
  Browser.closeTab(new_tab);
  is(selectui.hidden, true, "Select UI should be hidden");

  // We must finialize the tests
  finish();
}

function browserViewToClientRect(rect) {
  let container = document.getElementById("browsers");
  let containerBCR = container.getBoundingClientRect();
  return rect.clone().translate(Math.round(containerBCR.left), Math.round(containerBCR.top));
}
