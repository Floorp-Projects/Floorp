/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Load a web page containing an iframe that requires authentication but includes the X-Frame-Options: SAMEORIGIN header.
// Make sure that we don't needlessly show an authentication prompt for it.

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

add_task(async function () {
  SpecialPowers.pushPrefEnv({
    set: [["network.auth.supress_auth_prompt_for_XFO_failures", true]],
  });

  let URL =
    "https://example.com/browser/netwerk/test/browser/test_1629307.html";

  let hasPrompt = false;

  PromptTestUtils.handleNextPrompt(
    window,
    {
      modalType: Services.prefs.getIntPref("prompts.modalType.httpAuth"),
      promptType: "promptUserAndPass",
    },
    { buttonNumClick: 1 }
  )
    .then(function () {
      hasPrompt = true;
    })
    .catch(function () {});

  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, URL);

  // wait until the page and its iframe page is loaded
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, true, URL);

  Assert.equal(
    hasPrompt,
    false,
    "no prompt when loading page via iframe with x-auth options"
  );
});

add_task(async function () {
  SpecialPowers.pushPrefEnv({
    set: [["network.auth.supress_auth_prompt_for_XFO_failures", false]],
  });

  let URL =
    "https://example.com/browser/netwerk/test/browser/test_1629307.html";

  let hasPrompt = false;

  PromptTestUtils.handleNextPrompt(
    window,
    {
      modalType: Services.prefs.getIntPref("prompts.modalType.httpAuth"),
      promptType: "promptUserAndPass",
    },
    { buttonNumClick: 1 }
  )
    .then(function () {
      hasPrompt = true;
    })
    .catch(function () {});

  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, URL);

  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, true, URL);

  Assert.equal(
    hasPrompt,
    true,
    "prompt when loading page via iframe with x-auth options with pref network.auth.supress_auth_prompt_for_XFO_failures disabled"
  );
});
