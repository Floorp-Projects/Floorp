/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let modalType = Services.prefs.getIntPref("prompts.modalType.httpAuth");

add_task(async function test() {
  let tab = BrowserTestUtils.addTab(gBrowser);
  isnot(tab, gBrowser.selectedTab, "New tab shouldn't be selected");

  let authPromptShown = PromptTestUtils.waitForPrompt(tab.linkedBrowser, {
    modalType,
    promptType: "promptUserAndPass",
  });

  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "https://example.com/browser/toolkit/components/passwordmgr/test/browser/authenticate.sjs"
  );

  // Wait for the basic auth prompt
  let dialog = await authPromptShown;

  Assert.equal(gBrowser.selectedTab, tab, "Should have selected the new tab");

  // Cancel the auth prompt
  PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 1 });

  // After closing the prompt the load should finish
  await loadPromise;

  gBrowser.removeTab(tab);
});
