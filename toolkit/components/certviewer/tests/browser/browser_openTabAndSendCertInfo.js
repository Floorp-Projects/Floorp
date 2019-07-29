/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF = "security.aboutcertificate.enabled";

function checkAndClickButton(document, id) {
  let button = document.getElementById(id);
  Assert.ok(button, `${id} button found`);
  Assert.equal(
    button.hasAttribute("disabled"),
    false,
    "button should be clickable"
  );
  button.click();
}

function checksCertTab(tabsCount) {
  Assert.equal(gBrowser.tabs.length, tabsCount + 1, "New tab was opened");
  let spec = gBrowser.tabs[tabsCount].linkedBrowser.documentURI.spec;
  Assert.ok(
    spec.startsWith("about:certificate"),
    "about:certificate is the new opened tab"
  );

  let newTabUrl = new URL(spec);
  let certEncoded = newTabUrl.searchParams.get("cert");
  let certDecoded = decodeURIComponent(certEncoded);
  Assert.equal(
    btoa(atob(certDecoded)),
    certDecoded,
    "We received an base64 string"
  );
}

add_task(async function testBadCert() {
  info("Testing bad cert");
  let url = "https://expired.example.com/";
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

  SpecialPowers.pushPrefEnv({
    set: [[PREF, true]],
  });

  let tabsCount = gBrowser.tabs.length;
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

  gBrowser.removeCurrentTab(); // closes about:certificate
  gBrowser.removeCurrentTab(); // closes https://expired.example.com/
});

add_task(async function testGoodCert() {
  info("Testing page info");
  let url = "https://example.com/";

  SpecialPowers.pushPrefEnv({
    set: [[PREF, true]],
  });

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

    let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    checkAndClickButton(pageInfo.document, "security-view-cert");
    await loaded;

    pageInfo.close();
  });
  checksCertTab(tabsCount);

  gBrowser.removeCurrentTab(); // closes about:certificate
});

// Extracted from https://searchfox.org/mozilla-central/rev/40ef22080910c2e2c27d9e2120642376b1d8b8b2/browser/components/preferences/in-content/tests/head.js#8
function is_element_visible(aElement, aMsg) {
  isnot(aElement, null, "Element should not be null, when checking visibility");
  Assert.ok(!BrowserTestUtils.is_hidden(aElement), aMsg);
}

// Extracted from https://searchfox.org/mozilla-central/rev/40ef22080910c2e2c27d9e2120642376b1d8b8b2/browser/components/preferences/in-content/tests/head.js#41
function promiseLoadSubDialog(aURL) {
  return new Promise((resolve, reject) => {
    content.gSubDialog._dialogStack.addEventListener(
      "dialogopen",
      function dialogopen(aEvent) {
        if (
          aEvent.detail.dialog._frame.contentWindow.location == "about:blank"
        ) {
          return;
        }
        content.gSubDialog._dialogStack.removeEventListener(
          "dialogopen",
          dialogopen
        );

        Assert.equal(
          aEvent.detail.dialog._frame.contentWindow.location.toString(),
          aURL,
          "Check the proper URL is loaded"
        );

        // Check visibility
        is_element_visible(aEvent.detail.dialog._overlay, "Overlay is visible");

        // Check that stylesheets were injected
        let expectedStyleSheetURLs = aEvent.detail.dialog._injectedStyleSheets.slice(
          0
        );
        for (let styleSheet of aEvent.detail.dialog._frame.contentDocument
          .styleSheets) {
          let i = expectedStyleSheetURLs.indexOf(styleSheet.href);
          if (i >= 0) {
            info("found " + styleSheet.href);
            expectedStyleSheetURLs.splice(i, 1);
          }
        }
        Assert.equal(
          expectedStyleSheetURLs.length,
          0,
          "All expectedStyleSheetURLs should have been found"
        );

        // Wait for the next event tick to make sure the remaining part of the
        // testcase runs after the dialog gets ready for input.
        executeSoon(() => resolve(aEvent.detail.dialog._frame.contentWindow));
      }
    );
  });
}

add_task(async function testPreferencesCert() {
  info("Testing preferences cert");
  let url = "about:preferences#privacy";

  SpecialPowers.pushPrefEnv({
    set: [[PREF, true]],
  });

  let tabsCount;

  info(`Loading ${url}`);
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function(browser) {
    tabsCount = gBrowser.tabs.length;
    checkAndClickButton(browser.contentDocument, "viewCertificatesButton");

    let certDialogLoaded = promiseLoadSubDialog(
      "chrome://pippki/content/certManager.xul"
    );
    let dialogWin = await certDialogLoaded;
    let doc = dialogWin.document;
    Assert.ok(doc, "doc loaded");

    doc.getElementById("certmanagertabs").selectedTab = doc.getElementById(
      "ca_tab"
    );
    let treeView = doc.getElementById("ca-tree").view;
    let selectedCert;
    // See https://searchfox.org/mozilla-central/rev/40ef22080910c2e2c27d9e2120642376b1d8b8b2/browser/components/preferences/in-content/tests/browser_cert_export.js#41
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
    viewButton.click();
    await loaded;

    checksCertTab(tabsCount);
  });
  gBrowser.removeCurrentTab(); // closes about:certificate
});
