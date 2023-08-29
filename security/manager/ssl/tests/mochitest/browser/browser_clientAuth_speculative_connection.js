/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Tests that with speculative connections enabled, connections to servers that
// request a client authentication certificate succeed (the specific bug that
// was addressed with this patch involved navigation hanging because the
// connection to the server couldn't make progress without asking for a client
// authentication certificate, but it also wouldn't ask for a client
// authentication certificate until the connection had been claimed, which
// required that it make progress first).

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

let chooseCertificateCalled = false;

const clientAuthDialogService = {
  chooseCertificate(hostname, certArray, loadContext, callback) {
    is(
      certArray.length,
      1,
      "should have only one client certificate available"
    );
    ok(
      !chooseCertificateCalled,
      "chooseCertificate should only be called once"
    );
    chooseCertificateCalled = true;
    callback.certificateChosen(certArray[0], false);
  },

  QueryInterface: ChromeUtils.generateQI(["nsIClientAuthDialogService"]),
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable speculative connections.
      ["network.http.speculative-parallel-limit", 6],
      // Always ask to select a client authentication certificate.
      ["security.default_personal_cert", "Ask Every Time"],
    ],
  });
  let clientAuthDialogServiceCID = MockRegistrar.register(
    "@mozilla.org/security/ClientAuthDialogService;1",
    clientAuthDialogService
  );
  registerCleanupFunction(async function () {
    MockRegistrar.unregister(clientAuthDialogServiceCID);
  });
});

add_task(
  async function test_no_client_auth_selection_dialog_for_speculative_connections() {
    await BrowserTestUtils.withNewTab(
      `${TEST_PATH}browser_clientAuth_speculative_connection.html`,
      async browser => {
        // Click the link to navigate to a page that requests a client
        // authentication certificate. Necko will make a speculative
        // connection, but unfortunately there's no event or notification to
        // observe. This test ensures that the navigation succeeds and that a
        // client authentication certificate was requested.
        let loaded = BrowserTestUtils.browserLoaded(
          browser,
          false,
          "https://requireclientcert.example.com/"
        );
        await BrowserTestUtils.synthesizeMouseAtCenter("#link", {}, browser);
        await loaded;
        ok(chooseCertificateCalled, "chooseCertificate must have been called");
      }
    );
  }
);
