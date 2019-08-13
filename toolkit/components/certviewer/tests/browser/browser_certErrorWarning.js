/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF = "security.aboutcertificate.enabled";

let items = [
  {
    url: "https://expired.example.com/",
    title: "expired certificate",
    highlighted: [
      {
        group: "validity",
        infoItem: "not-after",
      },
    ],
  },
  {
    url: "https://untrusted.example.com/",
    title: "untrusted certificate",
    highlighted: [
      {
        group: "issuer-name",
      },
    ],
  },
  {
    url: "https://self-signed.example.com/",
    title: "self-signed certificate",
    highlighted: [
      {
        group: "issuer-name",
      },
    ],
  },
  {
    url: "https://no-subject-alt-name.example.com/",
    title: "invalid host certificate",
    highlighted: [
      {
        group: "subject-alt-names",
      },
      {
        group: "subject-name",
        infoItem: "common-name",
      },
    ],
  },
];

async function checkAssertions(browser) {
  await ContentTask.spawn(browser, null, async function() {
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document
          .querySelector("certificate-section")
          .shadowRoot.querySelector("warning-section"),
      "Warning section should be visible."
    );

    Assert.ok(
      content.document
        .querySelector("certificate-section")
        .shadowRoot.querySelector("warning-section"),
      "Warning section found."
    );
  });
}

async function openViaPage(url, title, highlighted) {
  info(`Testing to check for warning section on ${title} page.`);

  let browser;
  let certErrorLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
      browser = gBrowser.selectedBrowser;
      certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the cert error");
  await certErrorLoaded;

  let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  await ContentTask.spawn(browser, null, async function() {
    let advancedButton = content.document.getElementById("advancedButton");
    Assert.ok(advancedButton, "advancedButton found");
    Assert.equal(
      advancedButton.hasAttribute("disabled"),
      false,
      "advancedButton should be clickable"
    );
    advancedButton.click();

    let viewCertificate = content.document.getElementById("viewCertificate");
    Assert.equal(
      viewCertificate.hasAttribute("disabled"),
      false,
      "viewCertificate should be clickable"
    );
    viewCertificate.click();
  });
  await loaded;

  browser = gBrowser.selectedBrowser;

  info("Checking for warning section.");
  await checkAssertions(browser);

  await ContentTask.spawn(browser, { highlighted }, async function(
    highlighted
  ) {
    for (let i = 0; i < highlighted.length; i++) {
      let item = highlighted[i];
      let group = document
        .querySelector("certificate-section")
        .shadowRoot.querySelector("." + item.group);
      if (item.infoItem) {
        let infoItem = group.shadowRoot.querySelector("." + item.infoItem);
        Assert.ok(
          infoItem.classList.contains("warning"),
          "Warning item found."
        );
      } else {
        Assert.ok(group.classList.contains("warning"), "Warning item found.");
      }
    }
  });

  gBrowser.removeCurrentTab(); // closes about:certificate
  gBrowser.removeCurrentTab(); // closes https://expired.example.com/
}

// Test checks for warning section, when accessing about:certificate
// from about:certerror via the page's "view certificate" button.
add_task(async function test_checkForWarning_cerError_UI() {
  info("Testing to check for warning section.");
  await SpecialPowers.pushPrefEnv({
    set: [[PREF, true]],
  });
  for (let i = 0; i < items.length; i++) {
    await openViaPage(items[i].url, items[i].title, items[i].highlighted);
  }
});

async function openViaPageInfo(url, title) {
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  await pageLoaded;

  let pageInfo = BrowserPageInfo(url, "securityTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let viewCert = pageInfo.document.getElementById("security-view-cert");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(viewCert),
    "View certificate button should be visible."
  );

  let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  viewCert.click();

  await loaded;

  browser = gBrowser.selectedBrowser;

  info("Checking for warning section.");
  await checkAssertions(browser);

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
  pageInfo.close();
}

// Test checks for warning section, when accessing about:certificate
// from about:certerror via the pageInfo's "view certificate" button.
add_task(async function test_checkForWarning_certError_pageInfo() {
  info("Testing to check for warning section via pageInfo.");

  for (let i = 0; i < items.length; i++) {
    await openViaPageInfo(items[i].url, items[i].title);
  }
});

// Tests for warning UI from MITM software detection-related error
// page, via the page's "view certificate" button and pageInfo window
add_task(async function test_checkForWarning_mitmError_page() {
  const mitmEndpoint = "https://untrusted.example.com/";
  const mitmPref = "security.certerrors.mitm.priming.endpoint";

  SpecialPowers.pushPrefEnv({
    set: [[mitmPref, mitmEndpoint]],
  });

  await openViaPage(mitmEndpoint, "suspected mitm software");

  await openViaPageInfo(mitmEndpoint, "suspected mitm software");
});

// Tests for warning UI from MITM software detection-related error
// page, via the page's "view certificate" button and pageInfo window
add_task(async function test_checkForWarning_sha1Error_page() {
  SpecialPowers.pushPrefEnv({
    set: [["security.pki.sha1_enforcement_level", 1]],
  });

  await openViaPage(
    "https://sha1ee.example.com/browser/toolkit/components/certviewer/tests/browser/browser_certErrorWarning.js",
    "sha1"
  );

  await openViaPageInfo(
    "https://sha1ee.example.com/browser/toolkit/components/certviewer/tests/browser/browser_certErrorWarning.js",
    "sha1"
  );
});
