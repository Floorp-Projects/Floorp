/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let modalType = Services.prefs.getIntPref("prompts.modalType.httpAuth");

add_task(async function test() {
  await new Promise(resolve => {
    let tab = BrowserTestUtils.addTab(gBrowser);
    isnot(tab, gBrowser.selectedTab, "New tab shouldn't be selected");

    let authPromptShown = PromptTestUtils.waitForPrompt(tab.linkedBrowser, {
      modalType,
      promptType: "promptUserAndPass",
    });
    registerCleanupFunction(() => {
      gBrowser.removeTab(tab);
    });

    BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => finish());
    BrowserTestUtils.loadURI(
      tab.linkedBrowser,
      "http://example.com/browser/toolkit/components/passwordmgr/test/browser/authenticate.sjs"
    );

    authPromptShown.then(dialog => {
      is(gBrowser.selectedTab, tab, "Should have selected the new tab");
      PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 1 }); // Cancel
    });
  });
});
