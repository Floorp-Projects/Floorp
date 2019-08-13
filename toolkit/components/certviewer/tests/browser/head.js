/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function getErrorPage(url) {
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
  return browser;
}

async function openViaPageInfo(url) {
  await getErrorPage(url);

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
}

async function openViaErrorPage(url) {
  let browser = await getErrorPage(url);

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
}
