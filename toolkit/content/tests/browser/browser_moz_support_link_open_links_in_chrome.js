/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* 
  Ensures that the moz-support-link opens links correctly when
  this widget is used in the chrome.
*/

async function createMozSupportLink() {
  await import("chrome://global/content/elements/moz-support-link.mjs");
  let supportLink = document.createElement("a", { is: "moz-support-link" });
  supportLink.setAttribute("support-page", "dnt");
  let navigatorToolbox = document.getElementById("navigator-toolbox");
  navigatorToolbox.appendChild(supportLink);

  // If we do not wait for the element to be translated,
  // then there will be no visible text.
  await document.l10n.translateElements([supportLink]);
  return supportLink;
}

add_task(async function test_open_link_in_chrome_with_keyboard() {
  let supportTab;
  // Open link with Enter key
  let supportLink = await createMozSupportLink();
  supportLink.focus();
  const supportTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    Services.urlFormatter.formatURLPref("app.support.baseURL") + "dnt"
  );
  await EventUtils.synthesizeKey("KEY_Enter");
  supportTab = await supportTabPromise;
  Assert.ok(supportTab, "Support tab in new tab opened with Enter key");

  await BrowserTestUtils.removeTab(supportTab);

  let supportWindow;

  // Open link with Shift + Enter key combination
  supportLink.focus();
  const supportWindowPromise = BrowserTestUtils.waitForNewWindow(
    Services.urlFormatter.formatURLPref("app.support.baseURL") + "dnt"
  );
  await EventUtils.synthesizeKey("KEY_Enter", { shiftKey: true });
  supportWindow = await supportWindowPromise;
  Assert.ok(
    supportWindow,
    "Support tab in new window opened with Shift+Enter key"
  );
  supportLink.remove();
  await BrowserTestUtils.closeWindow(supportWindow);
});

add_task(async function test_open_link_in_chrome_with_mouse() {
  let supportTab;
  // Open link with mouse click

  let supportLink = await createMozSupportLink();
  const supportTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    Services.urlFormatter.formatURLPref("app.support.baseURL") + "dnt"
  );
  // This synthesize call works if you add a debugger statement before the call.
  EventUtils.synthesizeMouseAtCenter(supportLink, {});

  supportTab = await supportTabPromise;
  Assert.ok(supportTab, "Support tab in new tab opened");

  await BrowserTestUtils.removeTab(supportTab);

  let supportWindow;

  // Open link with Shift + mouse click combination
  const supportWindowPromise = BrowserTestUtils.waitForNewWindow(
    Services.urlFormatter.formatURLPref("app.support.baseURL") + "dnt"
  );
  await EventUtils.synthesizeMouseAtCenter(supportLink, { shiftKey: true });
  supportWindow = await supportWindowPromise;
  Assert.ok(supportWindow, "Support tab in new window opened");
  await BrowserTestUtils.closeWindow(supportWindow);
  supportLink = document.querySelector("a[support-page]");
  supportLink.remove();
});
