/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

async function checkServerCertificates(win, expectedValues = []) {
  await TestUtils.waitForCondition(() => {
    return (
      win.document.getElementById("serverList").itemChildren.length ==
      expectedValues.length
    );
  }, `Expected to have ${expectedValues.length} but got ${win.document.getElementById("serverList").itemChildren.length}`);
  await new Promise(win.requestAnimationFrame);

  let labels = win.document
    .getElementById("serverList")
    .querySelectorAll("label");

  // The strings we will get from the DOM are localized with Fluent.
  // This will wait until the translation is applied.
  if (expectedValues.length) {
    await BrowserTestUtils.waitForCondition(
      () => labels[1].value || !!labels[1].textContent.length,
      "At least one label is populated"
    );
  }

  expectedValues.forEach((item, i) => {
    let hostPort = labels[i * 3].value;
    let fingerprint = labels[i * 3 + 1].value || labels[i * 3 + 1].textContent;

    Assert.equal(
      hostPort,
      item.hostPort,
      `Expected override to be ${item.hostPort} but got ${hostPort}`
    );

    Assert.equal(
      fingerprint,
      item.fingerprint,
      `Expected override to have field ${item.fingerprint}`
    );
  });
}

async function deleteOverride(win, expectedLength) {
  win.document.getElementById("serverList").selectedIndex = 0;
  await TestUtils.waitForCondition(() => {
    return (
      win.document.getElementById("serverList").itemChildren.length ==
      expectedLength
    );
  });
  let newWinPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  // Since the .click() blocks we need to dispatch it to the main thread avoid that.
  Services.tm.dispatchToMainThread(() =>
    win.document.getElementById("websites_deleteButton").click()
  );
  let newWin = await newWinPromise;
  newWin.document.getElementById("deleteCertificate").acceptDialog();
  Assert.equal(
    win.document.getElementById("serverList").selectedIndex,
    0,
    "After deletion we expect the selectedItem to be reset."
  );
}

add_task(async function test_cert_manager_server_tab() {
  let win = await openCertManager();

  await checkServerCertificates(win);

  win.document.getElementById("certmanager").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  let cert = await readCertificate("md5-ee.pem", ",,");
  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  certOverrideService.rememberValidityOverride(
    "example.com",
    443,
    {},
    cert,
    false
  );

  win = await openCertManager();

  await checkServerCertificates(win, [
    {
      hostPort: "example.com:443",
      fingerprint: cert.sha256Fingerprint,
    },
  ]);

  await deleteOverride(win, 1);

  await checkServerCertificates(win, []);

  win.document.getElementById("certmanager").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  certOverrideService.clearAllOverrides();
});
