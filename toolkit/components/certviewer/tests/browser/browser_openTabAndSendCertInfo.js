/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
const TEST_CERT_BASE64 =
  "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";

function checkSpec(spec) {
  Assert.ok(
    spec.startsWith("about:certificate"),
    "about:certificate was opened"
  );

  let newUrl = new URL(spec);
  let certEncoded = newUrl.searchParams.get("cert");
  let certDecoded = decodeURIComponent(certEncoded);
  Assert.equal(
    btoa(atob(certDecoded)),
    certDecoded,
    "We received an base64 string"
  );
}

function checksCertTab(tabsCount) {
  Assert.equal(gBrowser.tabs.length, tabsCount + 1, "New tab was opened");
  let spec = gBrowser.tabs[tabsCount].linkedBrowser.documentURI.spec;
  checkSpec(spec);
}

async function checkCertChain(browser) {
  await SpecialPowers.spawn(browser, [], async function() {
    let certificateTabs;
    await ContentTaskUtils.waitForCondition(() => {
      let certificateSection = content.document.querySelector(
        "certificate-section"
      );
      if (!certificateSection) {
        return false;
      }
      certificateTabs = certificateSection.shadowRoot.querySelectorAll(
        ".certificate-tab"
      );
      return certificateTabs.length;
    }, "Found certificate tabs.");

    Assert.greaterOrEqual(
      certificateTabs.length,
      1,
      "Certificate chain tabs shown"
    );
  });
}

// taken from https://searchfox.org/mozilla-central/rev/7ed8e2d3d1d7a1464ba42763a33fd2e60efcaedc/security/manager/ssl/tests/mochitest/browser/browser_downloadCert_ui.js#47
function openCertDownloadDialog(cert) {
  let returnVals = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
    Ci.nsIWritablePropertyBag2
  );
  let win = window.openDialog(
    "chrome://pippki/content/downloadcert.xhtml",
    "",
    "",
    cert,
    returnVals
  );
  return new Promise((resolve, reject) => {
    win.addEventListener(
      "load",
      function() {
        executeSoon(() => resolve([win]));
      },
      { once: true }
    );
  });
}

add_task(async function openFromPopUp() {
  info("Testing openFromPopUp");

  const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  let cert = certdb.constructX509FromBase64(TEST_CERT_BASE64);

  let [win] = await openCertDownloadDialog(cert);
  let viewCertButton = win.document.getElementById("viewC-button");
  let newWinOpened = BrowserTestUtils.waitForNewWindow();
  viewCertButton.click();

  let topWin = await newWinOpened;
  let browser = topWin.gBrowser.selectedBrowser;

  // We're racing against messages sent up from the content process that
  // loads about:certificate. It may or may not have had the opportunity
  // to tell the parent that it had loaded the page yet. If not, we wait
  // for the page to load.
  //
  // Note that we can't use the URL parameter for
  // BrowserTestUtils.waitForNewWindow, since we need to use a function
  // to choose the right URL.
  if (!browser.currentURI.spec.startsWith("about:certificate")) {
    await BrowserTestUtils.browserLoaded(browser, false, spec =>
      spec.startsWith("about:certificate")
    );
  }

  let spec = browser.currentURI.spec;
  checkSpec(spec);

  await checkCertChain(browser);

  await BrowserTestUtils.closeWindow(topWin); // closes about:certificate
  win.document.getElementById("download_cert").cancelDialog();
  await BrowserTestUtils.windowClosed(win);
});

add_task(async function testBadCert() {
  info("Testing bad cert");

  let tab = await openErrorPage();

  let tabsCount = gBrowser.tabs.length;
  let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
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
  checksCertTab(tabsCount);
  await checkCertChain(gBrowser.selectedBrowser);

  gBrowser.removeCurrentTab(); // closes about:certificate
  gBrowser.removeCurrentTab(); // closes https://expired.example.com/
});

add_task(async function testBadCertIframe() {
  info("Testing bad cert in an iframe");

  let tab = await openErrorPage(true);

  let tabsCount = gBrowser.tabs.length;
  let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    let doc = content.document.querySelector("iframe").contentDocument;
    let advancedButton = doc.getElementById("advancedButton");
    Assert.ok(advancedButton, "advancedButton found");
    Assert.equal(
      advancedButton.hasAttribute("disabled"),
      false,
      "advancedButton should be clickable"
    );
    advancedButton.click();
    let viewCertificate = doc.getElementById("viewCertificate");
    Assert.ok(viewCertificate, "viewCertificate found");
    Assert.equal(
      viewCertificate.hasAttribute("disabled"),
      false,
      "viewCertificate should be clickable"
    );

    viewCertificate.click();
  });
  await loaded;
  checksCertTab(tabsCount);
  await checkCertChain(gBrowser.selectedBrowser);

  gBrowser.removeCurrentTab(); // closes about:certificate
  gBrowser.removeCurrentTab(); // closes https://expired.example.com/
});

add_task(async function testGoodCert() {
  info("Testing page info");
  let url = "https://example.com/";

  let tabsCount = gBrowser.tabs.length;

  info(`Loading ${url}`);
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function() {
    info("Opening pageinfo");
    let pageInfo = BrowserPageInfo(url, "securityTab", {});
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let securityTab = pageInfo.document.getElementById("securityTab");
    await TestUtils.waitForCondition(
      () => BrowserTestUtils.is_visible(securityTab),
      "Security tab should be visible."
    );
    Assert.ok(securityTab, "Security tab is available");
    let viewCertButton = pageInfo.document.getElementById("security-view-cert");
    await TestUtils.waitForCondition(
      () => BrowserTestUtils.is_visible(viewCertButton),
      "view cert button should be visible."
    );

    let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    checkAndClickButton(pageInfo.document, "security-view-cert");
    await loaded;

    await checkCertChain(gBrowser.selectedBrowser);
    pageInfo.close();
  });
  checksCertTab(tabsCount);

  gBrowser.removeCurrentTab(); // closes about:certificate
});

add_task(async function testPreferencesCert() {
  info("Testing preferences cert");
  let url = "about:preferences#privacy";

  let tabsCount;

  info(`Loading ${url}`);
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function(browser) {
    tabsCount = gBrowser.tabs.length;
    checkAndClickButton(browser.contentDocument, "viewCertificatesButton");

    let certDialogLoaded = promiseLoadSubDialog(
      "chrome://pippki/content/certManager.xhtml"
    );
    let dialogWin = await certDialogLoaded;
    let doc = dialogWin.document;
    Assert.ok(doc, "doc loaded");

    doc.getElementById("certmanagertabs").selectedTab = doc.getElementById(
      "mine_tab"
    );
    let treeView = doc.getElementById("user-tree").view;
    let selectedCert;
    // See https://searchfox.org/mozilla-central/rev/40ef22080910c2e2c27d9e2120642376b1d8b8b2/browser/components/preferences/in-content/tests/browser_cert_export.js#41
    for (let i = 0; i < treeView.rowCount; i++) {
      treeView.selection.select(i);
      dialogWin.getSelectedCerts();
      let certs = dialogWin.selected_certs;
      if (certs && certs[0]) {
        selectedCert = certs[0];
        break;
      }
    }
    Assert.ok(selectedCert, "A cert should be selected");
    let viewButton = doc.getElementById("mine_viewButton");
    Assert.equal(viewButton.disabled, false, "Should enable view button");

    let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    viewButton.click();
    await loaded;

    checksCertTab(tabsCount);
    await checkCertChain(gBrowser.selectedBrowser);
  });
  gBrowser.removeCurrentTab(); // closes about:certificate
});
