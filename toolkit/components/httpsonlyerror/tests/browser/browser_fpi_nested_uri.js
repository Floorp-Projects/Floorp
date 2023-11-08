/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a nested URI (in this case `view-source:`) does not result
// in a redirect loop when HTTPS-Only and First Party Isolation are
// enabled (Bug 1855734).

const INSECURE_VIEW_SOURCE_URL = "view-source:http://123.123.123.123/";

function promiseIsErrorPage() {
  return new Promise(resolve => {
    BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser).then(() =>
      resolve(true)
    );
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() =>
      resolve(false)
    );
  });
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode", true],
      ["dom.security.https_only_mode.upgrade_local", true],
      ["privacy.firstparty.isolate", true],
    ],
  });

  let loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  info(`Starting to load ${INSECURE_VIEW_SOURCE_URL}`);
  BrowserTestUtils.startLoadingURIString(gBrowser, INSECURE_VIEW_SOURCE_URL);
  await loaded;
  info(`${INSECURE_VIEW_SOURCE_URL} finished loading`);

  loaded = promiseIsErrorPage();
  await waitForAndClickOpenInsecureButton(gBrowser.selectedBrowser);
  info(`Waiting for normal or error page to load`);
  const isErrorPage = await loaded;

  ok(!isErrorPage, "We should not land on an error page");

  await Services.perms.removeAll();
});
