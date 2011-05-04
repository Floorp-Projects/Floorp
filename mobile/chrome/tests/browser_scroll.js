// Test behavior of window.scrollTo during page load (bug 654122).
"use strict";

var gTab;
registerCleanupFunction(function() Browser.closeTab(gTab));

const BASE_URL = "http://mochi.test:8888/browser/mobile/chrome/";
const TEST_URL = BASE_URL + "browser_scroll.html";

function test() {
  waitForExplicitFinish();
  gTab = Browser.addTab(TEST_URL, true);
  onMessageOnce(gTab.browser.messageManager, "Browser:FirstPaint", function() {
    executeSoon(function() {
      let rect = Elements.browsers.getBoundingClientRect();
      is(rect.top, 0, "Titlebar is hidden.");
    });
  });
}
