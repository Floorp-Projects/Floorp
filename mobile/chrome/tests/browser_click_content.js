let testURL_click = "chrome://mochikit/content/browser/mobile/chrome/browser_click_content.html";

let newTab;
let element;
let isClickFired = false;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();

  // Add new tab
  newTab = Browser.addTab(testURL_click, true);
  ok(newTab, "Tab Opened");

  // Wait for tab load (need to check the tab "loading", not the document "loading")
  waitFor(testClick, function() { return newTab.isLoading() == false; });
}

function clickFired() {
  isClickFired = true;
}

function testClick() {
  // Do sanity tests
  let uri = newTab.browser.currentURI.spec;
  is(uri, testURL_click, "URL Matches newly created Tab");

  // Check click
  element = newTab.browser.contentDocument.querySelector("iframe");
  element.addEventListener("click", clickFired, true);
  EventUtils.synthesizeMouseForContent(element, 1, 1, {}, window);
  waitFor(checkClick, function() { return isClickFired });
}

function checkClick() {
  ok(isClickFired, "Click handler fired");
  close();
}

function close() {
  // Close the tab
  Browser.closeTab(newTab);

  // Remove the listener
  element.removeEventListener("click", clickFired, true);

  // We must finialize the tests
  finish();
}
