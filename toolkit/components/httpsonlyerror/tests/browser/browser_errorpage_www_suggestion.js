/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
requestLongerTimeout(2);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  ""
);
const HTML_PATH = "/file_errorpage_www_suggestion.html";
const KICK_OF_REQUEST_WITH_SUGGESTION =
  "http://suggestion-example.com" + TEST_PATH + HTML_PATH;

add_task(async function () {
  info("Check that the www button shows up and leads to a secure www page");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode", true],
      ["dom.security.https_only_mode_send_http_background_request", false],
      ["dom.security.https_only_mode_error_page_user_suggestions", true],
    ],
  });

  let browser = gBrowser.selectedBrowser;
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(browser);
  BrowserTestUtils.startLoadingURIString(
    browser,
    KICK_OF_REQUEST_WITH_SUGGESTION
  );
  await errorPageLoaded;

  let pageShownPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "pageshow",
    true
  );

  // There's an arbitrary interval of 2 seconds in which the background
  // request for the www page is made. we wait this out to ensure the
  // www button has shown up.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(c => setTimeout(c, 2000));

  await SpecialPowers.spawn(browser, [], async function () {
    let doc = content.document;
    let innerHTML = doc.body.innerHTML;
    let errorPageL10nId = "about-httpsonly-title-alert";
    let suggestionBoxL10nId = "about-httpsonly-suggestion-box-www-text";

    ok(innerHTML.includes(errorPageL10nId), "the error page should show up");
    ok(doc.documentURI.startsWith("about:httpsonlyerror"));
    ok(
      innerHTML.includes(suggestionBoxL10nId),
      "the suggestion box should show up"
    );

    // click on www button
    let wwwButton = content.document.getElementById("openWWW");
    ok(wwwButton !== null, "The www Button should be shown");

    if (!wwwButton) {
      ok(false, "We should not be here");
    } else {
      wwwButton.click();
    }
  });
  await pageShownPromise;
  await SpecialPowers.spawn(browser, [], async function () {
    let doc = content.document;
    let innerHTML = doc.body.innerHTML;
    ok(
      innerHTML.includes("You are now on the secure www. page"),
      "The secure page should be reached after clicking the button"
    );
    ok(doc.documentURI.startsWith("https://www."), "Page should be secure www");
  });
});
