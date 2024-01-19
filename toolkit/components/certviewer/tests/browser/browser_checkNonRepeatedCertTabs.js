/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function checkCertTabs() {
  let certificatePages = 0;
  for (let tab of gBrowser.tabs) {
    let spec = tab.linkedBrowser.documentURI.spec;
    if (spec.includes("about:certificate")) {
      certificatePages++;
    }
  }
  Assert.equal(certificatePages, 1, "Do not open repeated certificate pages!");
}

add_task(async function testBadCert() {
  info("Testing bad cert");

  let tab = await openErrorPage();

  let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  for (let i = 0; i < 2; i++) {
    // try opening two certificates that are the same
    await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
      let advancedButton = content.document.getElementById("advancedButton");
      Assert.ok(advancedButton, "advancedButton found");
      Assert.equal(
        advancedButton.hasAttribute("disabled"),
        false,
        "advancedButton should be clickable"
      );
      advancedButton.click();
      let viewCertificate = content.document.getElementById("viewCertificate");
      Assert.ok(viewCertificate, "viewCertificate found");
      Assert.equal(
        viewCertificate.hasAttribute("disabled"),
        false,
        "viewCertificate should be clickable"
      );

      viewCertificate.click();
    });
    await loaded;
  }
  checkCertTabs();

  gBrowser.removeCurrentTab(); // closes about:certificate
  gBrowser.removeCurrentTab(); // closes https://expired.example.com/
});

add_task(async function testGoodCert() {
  info("Testing page info");
  let url = "https://example.com/";

  info(`Loading ${url}`);
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function () {
    info("Opening pageinfo");
    let pageInfo = BrowserPageInfo(url, "securityTab", {});
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let securityTab = pageInfo.document.getElementById("securityTab");
    await TestUtils.waitForCondition(
      () => BrowserTestUtils.isVisible(securityTab),
      "Security tab should be visible."
    );
    Assert.ok(securityTab, "Security tab is available");
    let viewCertButton = pageInfo.document.getElementById("security-view-cert");
    await TestUtils.waitForCondition(
      () => BrowserTestUtils.isVisible(viewCertButton),
      "view cert button should be visible."
    );

    let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    for (let i = 0; i < 2; i++) {
      checkAndClickButton(pageInfo.document, "security-view-cert");
      await loaded;
    }

    pageInfo.close();
    checkCertTabs();
  });

  gBrowser.removeCurrentTab();
});

add_task(async function testPreferencesCert() {
  info("Testing preferences cert");
  let url = "about:preferences#privacy";

  info(`Loading ${url}`);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      checkAndClickButton(browser.contentDocument, "viewCertificatesButton");

      let certDialogLoaded = promiseLoadSubDialog(
        "chrome://pippki/content/certManager.xhtml"
      );
      let dialogWin = await certDialogLoaded;
      let doc = dialogWin.document;
      Assert.ok(doc, "doc loaded");

      doc.getElementById("certmanagertabs").selectedTab =
        doc.getElementById("ca_tab");
      let treeView = doc.getElementById("ca-tree").view;
      let selectedCert;

      for (let i = 0; i < treeView.rowCount; i++) {
        treeView.selection.select(i);
        dialogWin.getSelectedCerts();
        let certs = dialogWin.selected_certs;
        if (certs && certs.length == 1 && certs[0]) {
          selectedCert = certs[0];
          break;
        }
      }
      Assert.ok(selectedCert, "A cert should be selected");
      let viewButton = doc.getElementById("ca_viewButton");
      Assert.equal(viewButton.disabled, false, "Should enable view button");

      let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
      for (let i = 0; i < 2; i++) {
        viewButton.click();
        await loaded;
      }
      checkCertTabs();
    }
  );
  gBrowser.removeCurrentTab(); // closes about:certificate
});
