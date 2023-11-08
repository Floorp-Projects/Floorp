/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ROOT_PATH = getRootDirectory(gTestPath);
const EXPIRED_ROOT_PATH = ROOT_PATH.replace(
  "chrome://mochitests/content",
  "http://supports-insecure.expired.example.com"
);
const SECURE_ROOT_PATH = ROOT_PATH.replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const INSECURE_ROOT_PATH = ROOT_PATH.replace(
  "chrome://mochitests/content",
  "http://example.com"
);

// This is how this test works:
//
// +----[REQUEST]  https://file_upgrade_insecure_server.sjs?queryresult
// |
// |  +-[REQUEST]  http://file_upgrade_insecure_server.sjs?content
// |  |            -> Internal HTTPS redirect
// |  |
// |  +>[RESPONSE] Expired Certificate Response
// |               -> HTTPS-Only Mode Error Page shows up
// |               -> Click exception button
// |
// |  +-[REQUEST]  http://file_upgrade_insecure_server.sjs?content
// |  |
// |  +>[RESPONSE] Webpage with a bunch of sub-resources
// |               -> http://file_upgrade_insecure_ser^er.sjs?img
// |               -> http://file_upgrade_insecure_server.sjs?xhr
// |               -> http://file_upgrade_insecure_server.sjs?iframe
// |               -> etc.
// |
// +--->[RESPONSE] List of all recorded requests and whether they were loaded
//                 with HTTP or not (eg.: img-ok, xhr-ok, iframe-error, ...)

add_task(async function () {
  const testCases = ["default", "private", "firstpartyisolation"];
  for (let i = 0; i < testCases.length; i++) {
    // Call sjs-file with setup query-string and store promise
    let expectedQueries = new Set([
      "content",
      "img",
      "iframe",
      "xhr",
      "nestedimg",
    ]);

    const filesLoaded = setupFileServer();
    // Since we don't know when the server has saved all it's variables,
    // let's wait a bit before reloading the page.
    await new Promise(resolve => executeSoon(resolve));

    // Create a new private window but reuse the normal one.
    let privateWindow = false;
    if (testCases[i] === "private") {
      privateWindow = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
    } else if (testCases[i] === "firstpartyisolation") {
      await SpecialPowers.pushPrefEnv({
        set: [["privacy.firstparty.isolate", true]],
      });
    }

    // Create new tab with sjs-file requesting content.
    // "supports-insecure.expired.example.com" responds to http and https but
    // with an expired certificate
    let tab = await openErrorPage(
      `${EXPIRED_ROOT_PATH}file_upgrade_insecure_server.sjs?content`,
      false,
      privateWindow
    );
    let browser = tab.linkedBrowser;

    let pageShownPromise = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow",
      true
    );

    await waitForAndClickOpenInsecureButton(browser);

    await pageShownPromise;

    // Check if the original page got loaded with http this time
    await SpecialPowers.spawn(browser, [], async function () {
      let doc = content.document;
      ok(
        !doc.documentURI.startsWith("http://expired.example.com"),
        "Page should load normally after exception button was clicked."
      );
    });

    // Wait for initial sjs request to resolve
    let results = await filesLoaded;

    for (let resultIndex in results) {
      const response = results[resultIndex];
      // A response looks either like this "iframe-ok" or "[key]-[result]"
      const [key, result] = response.split("-", 2);
      // try to find the expected result within the results array
      if (expectedQueries.has(key)) {
        expectedQueries.delete(key);
        is(result, "ok", `Request '${key}' should be loaded with HTTP.'`);
      } else {
        ok(false, `Unexpected response from server (${response})`);
      }
    }

    // Clean up permissions, tab and potentially preferences
    Services.perms.removeAll();

    if (testCases[i] === "firstpartyisolation") {
      await SpecialPowers.popPrefEnv();
    }

    if (privateWindow) {
      await BrowserTestUtils.closeWindow(privateWindow);
    } else {
      gBrowser.removeCurrentTab();
    }
  }
});

function setupFileServer() {
  // We initialize the upgrade-server with the queryresult query-string.
  // We'll get a response once all files have been requested and then
  // can see if they have been requested with http.
  return new Promise((resolve, reject) => {
    var xhrRequest = new XMLHttpRequest();
    xhrRequest.open(
      "GET",
      `${SECURE_ROOT_PATH}file_upgrade_insecure_server.sjs?queryresult=${INSECURE_ROOT_PATH}`
    );
    xhrRequest.onload = function (e) {
      var results = xhrRequest.responseText.split(",");
      resolve(results);
    };
    xhrRequest.onerror = e => {
      ok(false, "Could not query results from server (" + e.message + ")");
      reject();
    };
    xhrRequest.send();
  });
}
