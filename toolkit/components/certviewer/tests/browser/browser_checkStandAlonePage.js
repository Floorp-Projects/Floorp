/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TYPE_UNKNOWN = 0;
const TYPE_CA = 1;
const TYPE_USER = 2;
const TYPE_EMAIL = 4;
const TYPE_SERVER = 8;
const TYPE_ANY = 0xffff;

add_task(async function test_certificateItems() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(
      gBrowser,
      "about:certificate"
    );
  });

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    let aboutCertificateSection = await ContentTaskUtils.waitForCondition(
      () => {
        return content.document.querySelector("about-certificate-section");
      },
      "Found aboutCertificateSection."
    );

    let certificateTabs = aboutCertificateSection.shadowRoot.querySelectorAll(
      ".certificate-tab"
    );
    let expected = [
      "Unknown",
      "Authorities",
      "Servers",
      "People",
      "Your Certificates",
    ];
    Assert.ok(certificateTabs, "Certificate tabs should exist.");
    Assert.equal(
      certificateTabs.length,
      expected.length,
      `There should be ${expected.length} tabs.`
    );
    let checked = []; // to avoid repeated tabs
    for (let tab of certificateTabs) {
      Assert.ok(
        expected.includes(tab.textContent) &&
          !checked.includes(tab.textContent),
        `${tab.textContent} should be one tab`
      );
      checked.push(tab.textContent);
    }

    let certificateItems = aboutCertificateSection.shadowRoot.querySelectorAll(
      "about-certificate-items"
    );
    Assert.ok(certificateItems, "Certificate items should exist.");
    Assert.equal(
      certificateItems.length,
      certificateTabs.length,
      `There should be ${certificateTabs.length} certificate items`
    );

    for (let item of certificateItems) {
      let listItems = item.shadowRoot.querySelectorAll("list-item");
      Assert.ok(listItems, "list items should exist");
    }
  });
  gBrowser.removeCurrentTab(); // closes about:certificate
});

async function checkItem(expected) {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(
      gBrowser,
      "about:certificate"
    );
  });

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [expected],
    async function(expected) {
      let aboutCertificateSection = await ContentTaskUtils.waitForCondition(
        () => {
          return content.document.querySelector("about-certificate-section");
        },
        "Found aboutCertificateSection."
      );

      let tab = aboutCertificateSection.shadowRoot.querySelector(
        `.certificate-tabs #certificate-viewer-tab-${expected.certificateItemsID}`
      );
      Assert.ok(tab, `${expected.tabName} tab should exist.`);
      tab.click();

      let certificateItems = aboutCertificateSection.shadowRoot.querySelector(
        `.info-groups #certificate-viewer-tab-${expected.certificateItemsID}`
      );

      let listItems = certificateItems.shadowRoot.querySelectorAll("list-item");
      if (expected.displayName == null) {
        Assert.equal(
          listItems.length,
          0,
          "There shouldn't be elements in the list"
        );
      } else {
        let found = false;
        for (let item of listItems) {
          let name = item.shadowRoot.querySelector(".item-name").textContent;
          if (name == expected.displayName) {
            found = true;
            break;
          }
        }
        Assert.equal(found, true, `${expected.displayName} should be listed`);
      }
    }
  );
  gBrowser.removeCurrentTab(); // closes about:certificate
}

add_task(async function test_dbItemDisplayed() {
  let certs = {
    [TYPE_UNKNOWN]: null,
    [TYPE_CA]: null,
    [TYPE_USER]: null,
    [TYPE_EMAIL]: null,
    [TYPE_SERVER]: null,
    [TYPE_ANY]: null,
  };
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  Assert.ok(certdb, "certdb not null");
  let certcache = certdb.getCerts();
  Assert.ok(certcache, "certcache not null");

  let totalSaved = 0;
  for (let cert of certcache) {
    if (totalSaved == certs.length) {
      break;
    }
    if (certs[cert.certType] == null) {
      totalSaved += 1;
      certs[cert.certType] = {
        displayName: cert.displayName,
      };
    }
  }

  let tests = [
    {
      tabName: "Unknown",
      displayName: certs[TYPE_UNKNOWN] ? certs[TYPE_UNKNOWN].displayName : null,
      certificateItemsID: "unkonwn",
    },
    {
      tabName: "Authorities",
      displayName: certs[TYPE_CA] ? certs[TYPE_CA].displayName : null,
      certificateItemsID: "ca",
    },
    {
      tabName: "Servers",
      displayName: certs[TYPE_SERVER] ? certs[TYPE_SERVER].displayName : null,
      certificateItemsID: "servers",
    },
    {
      tabName: "People",
      displayName: certs[TYPE_EMAIL] ? certs[TYPE_EMAIL].displayName : null,
      certificateItemsID: "people",
    },
    {
      tabName: "Your Certificates",
      displayName: certs[TYPE_USER] ? certs[TYPE_USER].displayName : null,
      certificateItemsID: "mine",
    },
  ];

  for (let test of tests) {
    await checkItem(test);
  }
});
