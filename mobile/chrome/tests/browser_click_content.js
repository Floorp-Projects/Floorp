let testURL_click = "chrome://mochikit/content/browser/mobile/chrome/browser_click_content.html";

let newTab;
let element;
let isClickFired = false;
let clickPosition = { x: null, y: null};
let isLoading = function() {
  return !newTab.isLoading() &&
    newTab.browser.currentURI.spec != "about:blank";
};

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();

  // Add new tab
  newTab = Browser.addTab(testURL_click, true);
  ok(newTab, "Tab Opened");

  // Wait for tab load (need to check the tab "loading", not the document "loading")
  waitFor(testClickAndPosition, isLoading);
}

function clickFired(aEvent) {
  isClickFired = true;
  let [x, y] = Browser.browserViewToClient(aEvent.clientX, aEvent.clientY);
  clickPosition.x = x;
  clickPosition.y = y;
}

function testClickAndPosition() {
  // Do sanity tests
  let uri = newTab.browser.currentURI.spec;
  is(uri, testURL_click, "URL Matches newly created Tab");

  // Check click
  element = newTab.browser.contentDocument.getElementById("iframe-1");
  element.addEventListener("click", clickFired, true);

  EventUtils.synthesizeMouseForContent(element, 1, 1, {}, window);
  waitFor(checkClick, function() { return isClickFired });
}

function checkClick() {
  ok(isClickFired, "Click handler fired");
  element.removeEventListener("click", clickFired, true);

  // Check position
  isClickFired = false;
  element = newTab.browser.contentDocument.documentElement;
  element.addEventListener("click", clickFired, true);

  finish(); // XXX Browser.getBoundingContentRect not available.

  let rect = Browser.getBoundingContentRect(element);
  EventUtils.synthesizeMouse(element, 1, rect.height + 10, {}, window);
  waitFor(checkPosition, function() { return isClickFired });
}

function checkPosition() {
  element.removeEventListener("click", clickFired, true);

  let rect = Browser.getBoundingContentRect(element);
  is(clickPosition.x, 1, "X position is correct");
  is(clickPosition.y, rect.height + 10, "Y position is correct");

  checkThickBorder();
}

function checkThickBorder() {
  let frame = newTab.browser.contentDocument.getElementById("iframe-2");
  let element = frame.contentDocument.getElementsByTagName("input")[0];

  let frameRect = Browser.getBoundingContentRect(frame);
  let frameLeftBorder = window.getComputedStyle(frame, "").borderLeftWidth;
  let frameTopBorder = window.getComputedStyle(frame, "").borderTopWidth;

  let elementRect = Browser.getBoundingContentRect(element);
  ok((frameRect.left + parseInt(frameLeftBorder)) < elementRect.left, "X position of nested element ok");
  ok((frameRect.top + parseInt(frameTopBorder)) < elementRect.top, "Y position of nested element ok");

  close();
}

function close() {
  // Close the tab
  Browser.closeTab(newTab);

  // We must finialize the tests
  finish();
}
