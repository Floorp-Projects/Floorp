/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TYPE_UNKNOWN = 0;
const TYPE_CA = 1;
const TYPE_USER = 2;
const TYPE_EMAIL = 4;
const TYPE_SERVER = 8;

add_task(async function test_dbItemDisplayed() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(
      gBrowser,
      "about:certificate"
    );
  });

  let categories = [
    {
      type: TYPE_UNKNOWN,
      tabName: "Unknown",
      id: "unkonwn",
    },
    {
      type: TYPE_CA,
      tabName: "Authorities",
      id: "ca",
    },
    {
      type: TYPE_USER,
      tabName: "Your Certificates",
      id: "mine",
    },
    {
      type: TYPE_EMAIL,
      tabName: "People",
      id: "people",
    },
    {
      type: TYPE_SERVER,
      tabName: "Servers",
      id: "servers",
    },
  ];

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  Assert.ok(certdb, "certdb not null");
  let certcache = certdb.getCerts();
  Assert.ok(certcache, "certcache not null");

  for (let cert of certcache) {
    let category = categories.find(({ type }) => type & cert.certType);

    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [cert.displayName, category],
      async function(displayName, category) {
        let aboutCertificateSection = await ContentTaskUtils.waitForCondition(
          () => {
            return content.document.querySelector("about-certificate-section");
          },
          "Found aboutCertificateSection."
        );

        let tab = aboutCertificateSection.shadowRoot.querySelector(
          `.certificate-tabs #certificate-viewer-tab-${category.id}`
        );
        Assert.ok(tab, `${category.tabName} tab should exist.`);
        tab.click();

        let certificateItems = aboutCertificateSection.shadowRoot.querySelector(
          `.info-groups #certificate-viewer-tab-${category.id}`
        );

        let listItems = certificateItems.shadowRoot.querySelectorAll(
          "list-item"
        );

        let item = Array.from(listItems).find(
          i =>
            i.shadowRoot.querySelector(".item-name").textContent == displayName
        );
        Assert.ok(item, `${displayName} should be listed`);
      }
    );
  }

  gBrowser.removeCurrentTab(); // closes about:certificate
});
