// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Messaging.jsm");

// The chrome window and friends.
let chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
let BrowserApp = chromeWin.BrowserApp;

const BASE = "https://example.com:443/tests/robocop/";
const XPI = BASE + "browser_theme_image_file.xpi";
const PAGE = BASE + "robocop_blank_04.html";

// Track the tabs where the tests are happening.
let tabTest;

function cleanupTabs() {
  if (tabTest) {
    BrowserApp.closeTab(tabTest);
    tabTest = null;
  }
}

add_task(async function testThemeInstall() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webapi.testing", true],
          ["extensions.install.requireBuiltInCerts", false]],
  });

  tabTest = BrowserApp.addTab(PAGE);
  let browser = tabTest.browser;
  let content = browser.contentWindow;
  await promiseBrowserEvent(browser, "load", { resolveAtNextTick: true });

  let updates = [];
  function observer(subject, topic, data) {
    updates.push(data);
  }
  Services.obs.addObserver(observer, "lightweight-theme-styling-update");

  let installPromise = promiseNotification("lightweight-theme-styling-update");
  let install = await content.navigator.mozAddonManager.createInstall({url: XPI});
  install.install();
  await installPromise;
  ok(true, "Theme install completed");

  is(updates.length, 1, "Got a single theme update");
  let parsed = JSON.parse(updates[0]);
  ok(parsed.theme.headerURL.endsWith("/testImage.png"),
     "Theme update has the expected headerURL");
  ok(parsed.theme.headerURL.startsWith("moz-extension"),
     "headerURL references resource inside extension file");

  await EventDispatcher.instance.sendRequestForResult({
    type: "Robocop:WaitOnUI",
  });

  Services.obs.removeObserver(observer, "lightweight-theme-styling-update");
  cleanupTabs();
});

run_next_test();
