//
// Bug 1725996 - Test if the cookie set in a document which is created by
//               document.open() in an about:blank iframe has a correct
//               partitionKey
//

const TEST_PAGE = TEST_DOMAIN + TEST_PATH + "file_iframe_document_open.html";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["privacy.dynamic_firstparty.use_site", true],
    ],
  });
});

add_task(async function test_firstParty_iframe() {
  // Clear all cookies before test.
  Services.cookies.removeAll();

  // Open the test page which creates an iframe and then calls document.write()
  // to write a cookie in the iframe.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  // Wait until the cookie appears.
  await TestUtils.waitForCondition(_ => Services.cookies.cookies.length);

  // Check the partitionKey in the cookie.
  let cookie = Services.cookies.cookies[0];
  is(
    cookie.originAttributes.partitionKey,
    "",
    "The partitionKey should remain empty for first-party iframe."
  );

  // Clean up.
  Services.cookies.removeAll();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_thirdParty_iframe() {
  // Clear all cookies before test.
  Services.cookies.removeAll();

  // Open a tab with a different domain with the test page.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE_2
  );

  // Open the test page within a third-party context.
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_PAGE],
    async function (page) {
      let ifr = content.document.createElement("iframe");
      let loading = ContentTaskUtils.waitForEvent(ifr, "load");
      ifr.src = page;
      content.document.body.appendChild(ifr);
      await loading;
    }
  );

  // Wait until the cookie appears.
  await TestUtils.waitForCondition(_ => Services.cookies.cookies.length);

  // Check the partitionKey in the cookie.
  let cookie = Services.cookies.cookies[0];
  is(
    cookie.originAttributes.partitionKey,
    "(http,xn--exmple-cua.test)",
    "The partitionKey should exist for third-party iframe."
  );

  // Clean up.
  Services.cookies.removeAll();
  BrowserTestUtils.removeTab(tab);
});
