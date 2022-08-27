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
    let certString = labels[i * 3 + 1].value || labels[i * 3 + 1].textContent;
    let isTemporaryString =
      labels[i * 3 + 2].value || labels[i * 3 + 2].textContent;

    Assert.equal(
      hostPort,
      item.hostPort,
      `Expected override to be ${item.hostPort} but got ${hostPort}`
    );

    Assert.equal(
      certString,
      item.certName,
      `Expected override to have field ${item.certName}`
    );

    Assert.equal(
      isTemporaryString,
      item.isTemporary ? "Temporary" : "Permanent",
      `Expected override to be ${item.isTemporary ? "Temporary" : "Permanent"}`
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

async function testViewButton(win) {
  win.document.getElementById("serverList").selectedIndex = 0;

  Assert.ok(
    !win.document.getElementById("websites_viewButton").disabled,
    "View button should be enabled for override with cert"
  );

  let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  win.document.getElementById("websites_viewButton").click();

  let newTab = await loaded;
  let spec = newTab.linkedBrowser.documentURI.spec;

  Assert.ok(
    spec.startsWith("about:certificate"),
    "about:certificate should habe been opened"
  );

  let newUrl = new URL(spec);
  let certEncoded = newUrl.searchParams.get("cert");
  let certDecoded = decodeURIComponent(certEncoded);
  Assert.ok(certDecoded, "should have some certificate as cert url param");

  gBrowser.removeCurrentTab();
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
      certName: "md5-ee",
      isTemporary: false,
    },
  ]);

  await testViewButton(win);

  await deleteOverride(win, 1);

  await checkServerCertificates(win, []);

  win.document.getElementById("certmanager").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  certOverrideService.clearAllOverrides();
});
