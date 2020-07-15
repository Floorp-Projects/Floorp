/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

async function checkServerCertificates(expectedNumberOfServerCertificates) {
  let win = await openCertManager();

  let serverCertsTreeView = win.document.getElementById("server-tree").view;
  let rowCount = serverCertsTreeView.rowCount;
  let nonContainerCount = 0;
  for (let i = 0; i < rowCount; i++) {
    if (!serverCertsTreeView.isContainer(i)) {
      nonContainerCount++;
    }
  }
  Assert.equal(
    nonContainerCount,
    expectedNumberOfServerCertificates,
    "should have the expected number of server certificates"
  );

  win.document.getElementById("certmanager").acceptDialog();
  await BrowserTestUtils.windowClosed(win);
}

add_task(async function test_no_spurrious_server_certificates() {
  await checkServerCertificates(0);

  let cert = await readCertificate("md5-ee.pem", ",,");
  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  certOverrideService.rememberValidityOverride(
    "example.com",
    443,
    cert,
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    false
  );

  await checkServerCertificates(1);

  certOverrideService.clearValidityOverride("example.com", 443);

  await checkServerCertificates(0);
});
