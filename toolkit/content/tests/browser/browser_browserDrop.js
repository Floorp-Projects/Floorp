/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let newTab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function () {
    gBrowser.removeTab(newTab);
  });

  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
  let ChromeUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

  let browser = gBrowser.selectedBrowser;

  var linkHandlerActivated = 0;
  // Don't worry about clobbering the droppedLinkHandler, since we're closing
  // this tab after the test anyways
  browser.droppedLinkHandler = function dlh(e, url, name) {
    linkHandlerActivated++;
    ok(!/(javascript|data)/i.test(url), "javascript link should not be dropped");
  }

  var receivedDropCount = 0;
  function dropListener() {
    receivedDropCount++;
    if (receivedDropCount == triggeredDropCount) {
      // Wait for the browser's system-phase event handler to run.
      executeSoon(function () {
        is(linkHandlerActivated, validDropCount,
           "link handler was called correct number of times");
        finish();
      })
    }
  }
  browser.addEventListener("drop", dropListener, false);
  registerCleanupFunction(function () {
    browser.removeEventListener("drop", dropListener, false);
  });

  var triggeredDropCount = 0;
  var validDropCount = 0;
  function drop(text, valid) {
    triggeredDropCount++;
    if (valid)
      validDropCount++;
    executeSoon(function () {
      ChromeUtils.synthesizeDrop(browser, browser, [[{type: "text/plain", data: text}]], "copy", window);
    });
  }

  drop("mochi.test/first", true);
  drop("javascript:'bad'");
  drop("jAvascript:'also bad'");
  drop("mochi.test/second", true);
  drop("data:text/html,bad");
  drop("mochi.test/third", true);
}
