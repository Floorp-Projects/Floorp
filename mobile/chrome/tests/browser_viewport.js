let testURL_blank = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_01.html";
let testURL_vport_01 = "chrome://mochikit/content/browser/mobile/chrome/browser_viewport_01.html";
let testURL_vport_02 = "chrome://mochikit/content/browser/mobile/chrome/browser_viewport_02.html";

let working_tab;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();

  // Add new tab
  working_tab = Browser.addTab(testURL_blank, true);
  ok(working_tab, "Tab Opened");

  // Check viewport load (need to check the tab "loading", not the document "loading")
  waitFor(load_first_blank, function() { return working_tab.isLoading() == false; });
}

function load_first_blank() {
  // Do sanity tests
  var uri = working_tab.browser.currentURI.spec;
  is(uri, testURL_blank, "URL Matches newly created Tab");

  // Check viewport settings
  ok(working_tab.browser.classList.contains("browser"), "Normal 'browser' class");
  let style = window.getComputedStyle(working_tab.browser, null);
  is(style.width, "800px", "Normal 'browser' width is 800 pixels");

  // Load device-width page
  BrowserUI.goToURI(testURL_vport_01);

  // Check viewport load (need to check the tab "loading", not the document "loading")
  waitFor(load_first_viewport, function() { return working_tab.isLoading() == false; });
}

function load_first_viewport() {
  // Do sanity tests
  var uri = working_tab.browser.currentURI.spec;
  is(uri, testURL_vport_01, "URL Matches newly created Tab");

  // Check viewport settings
  ok(working_tab.browser.classList.contains("browser-viewport"), "Viewport 'browser-viewport' class");
  let style = window.getComputedStyle(working_tab.browser, null);
  is(style.width, window.innerWidth + "px", "Viewport device-width is equal to window.innerWidth");

  is(Browser._browserView.getZoomLevel(), 1, "Viewport scale=1");

  // Load device-width page
  BrowserUI.goToURI(testURL_blank);

  // Check viewport load (need to check the tab "loading", not the document "loading")
  waitFor(load_second_blank, function() { return working_tab.isLoading() == false; });
}

function load_second_blank() {
  // Do sanity tests
  var uri = working_tab.browser.currentURI.spec;
  is(uri, testURL_blank, "URL Matches newly created Tab");

  // Check viewport settings
  ok(working_tab.browser.classList.contains("browser"), "Normal 'browser' class");
  let style = window.getComputedStyle(working_tab.browser, null);
  is(style.width, "800px", "Normal 'browser' width is 800 pixels");

  // Load width fixed page
  BrowserUI.goToURI(testURL_vport_02);

  // Check viewport load (need to check the tab "loading", not the document "loading")
  waitFor(load_second_viewport, function() { return working_tab.isLoading() == false; });
}

function load_second_viewport() {
  // Do sanity tests
  var uri = working_tab.browser.currentURI.spec;
  is(uri, testURL_vport_02, "URL Matches newly created Tab");

  // Check viewport settings
  ok(working_tab.browser.classList.contains("browser-viewport"), "Viewport 'browser-viewport' class");
  let style = window.getComputedStyle(working_tab.browser, null);
  let expectedWidth = Math.max(320, window.innerWidth);
  is(style.width, expectedWidth+"px", "Viewport width is at least 320"); // Bug 561413

  is(Browser._browserView.getZoomLevel(), 1, "Viewport scale=1");

  // Load device-width page
  BrowserUI.goToURI(testURL_blank);

  // Check viewport load (need to check the tab "loading", not the document "loading")
  waitFor(load_third_blank, function() { return working_tab.isLoading() == false; });
}

function load_third_blank() {
  // Do sanity tests
  var uri = working_tab.browser.currentURI.spec;
  is(uri, testURL_blank, "URL Matches newly created Tab");

  // Check viewport settings
  ok(working_tab.browser.classList.contains("browser"), "Normal 'browser' class");
  let style = window.getComputedStyle(working_tab.browser, null);
  is(style.width, "800px", "Normal 'browser' width is 800 pixels");

  // Close down the test
  test_close();
}

function test_close() {
  // Close the tab
  Browser.closeTab(working_tab);

  // We must finialize the tests
  finish();
}
