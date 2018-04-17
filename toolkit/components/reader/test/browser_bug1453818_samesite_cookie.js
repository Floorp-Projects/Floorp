/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ORIGIN1 = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");
const TEST_ORIGIN2 = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.org");

async function clickLink(browser) {
  info("Waiting for the page to load after clicking the link...");
  let pageLoaded = BrowserTestUtils.waitForContentEvent(browser, "DOMContentLoaded");
  await ContentTask.spawn(browser, null, async function() {
    let link = content.document.getElementById("link");
    ok(link, "The link element was found.");
    link.click();
  });
  await pageLoaded;
}

async function checkCookiePresent(browser) {
  await ContentTask.spawn(browser, null, async function() {
    let cookieSpan = content.document.getElementById("cookieSpan");
    ok(cookieSpan, "cookieSpan element should be in document");
    is(cookieSpan.textContent, "foo=bar", "The SameSite cookie was sent correctly.");
  });
}

async function checkCookie(sameSiteEnabled, browser) {
  if (sameSiteEnabled) {
    info("Check that the SameSite cookie was not sent.");
    await ContentTask.spawn(browser, null, async function() {
      let cookieSpan = content.document.getElementById("cookieSpan");
      ok(cookieSpan, "cookieSpan element should be in document");
      is(cookieSpan.textContent, "", "The SameSite cookie was blocked correctly.");
    });
  } else {
    info("Check that the SameSite cookie was sent.");
    await checkCookiePresent(browser);
  }
}

async function runTest(sameSiteEnabled) {
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.same-site.enabled", sameSiteEnabled],
          ["reader.parse-on-load.enabled", true]],
  });

  info("Set a SameSite=strict cookie.");
  await BrowserTestUtils.withNewTab(TEST_ORIGIN1 + "setSameSiteCookie.html", () => {});

  info("Check that the cookie has been correctly set.");
  await BrowserTestUtils.withNewTab(TEST_ORIGIN1 + "getCookies.html", async function(browser) {
    await checkCookiePresent(browser);
  });

  info("Open a cross-origin page with a link to the domain that set the cookie.");
  let browser;
  let pageLoaded;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    let t = BrowserTestUtils.addTab(gBrowser, TEST_ORIGIN2 + "linkToGetCookies.html");
    gBrowser.selectedTab = t;
    browser = gBrowser.selectedBrowser;
    pageLoaded = BrowserTestUtils.waitForContentEvent(browser, "DOMContentLoaded");
    return t;
  }, false);

  info("Waiting for the page to load in normal mode...");
  await pageLoaded;

  await clickLink(browser);
  await checkCookie(sameSiteEnabled, browser);
  await BrowserTestUtils.removeTab(tab);

  info("Open the cross-origin page again.");
  await BrowserTestUtils.withNewTab(TEST_ORIGIN2 + "linkToGetCookies.html", async function(browser) {
    let pageShown = BrowserTestUtils.waitForContentEvent(browser, "AboutReaderContentReady");
    let readerButton = document.getElementById("reader-mode-button");
    ok(readerButton, "readerButton should be available");
    readerButton.click();

    info("Waiting for the page to be displayed in reader mode...");
    await pageShown;

    await clickLink(browser);
    await checkCookie(sameSiteEnabled, browser);
  });
}

add_task(async function() {
  await runTest(true);
});

add_task(async function() {
  await runTest(false);
});
