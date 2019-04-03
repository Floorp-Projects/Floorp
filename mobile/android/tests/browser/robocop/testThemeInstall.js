// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {EventDispatcher} = ChromeUtils.import("resource://gre/modules/Messaging.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

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

// A user prompt is displayed during theme installation and responding to
// the prompt from test code is tricky.  Instead, just override the prompt
// implementation and let installation proceed.
const PROMPT_CLASSID = Components.ID("{85325f87-03f8-d142-b3a0-d2a0b8f2d4e0}");
const PROMPT_CONTRACTID = "@mozilla.org/addons/web-install-prompt;1";
function NullPrompt() {}
NullPrompt.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.amIWebInstallPrompt]),
  confirm(browser, url, installs) {
    installs[0].install();
  },
};

add_task(async function testThemeInstall() {
  let factory = XPCOMUtils.generateSingletonFactory(NullPrompt);
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  let originalCID = registrar.contractIDToCID(PROMPT_CONTRACTID);
  registrar.registerFactory(PROMPT_CLASSID, "", PROMPT_CONTRACTID, factory);

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
    updates.push(JSON.stringify(subject.wrappedJSObject));
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

  registrar.unregisterFactory(PROMPT_CLASSID, factory);
  registrar.registerFactory(originalCID, "", PROMPT_CONTRACTID, null);
});

run_next_test();
