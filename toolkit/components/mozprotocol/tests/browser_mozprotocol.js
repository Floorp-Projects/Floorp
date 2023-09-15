// Check that entering moz://a into the address bar directs us to a new url
add_task(async function () {
  let path = getRootDirectory(gTestPath).substring(
    "chrome://mochitests/content/".length
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "toolkit.mozprotocol.url",
        `https://example.com/${path}mozprotocol.html`,
      ],
    ],
  });

  await BrowserTestUtils.withNewTab("about:blank", async function () {
    BrowserTestUtils.startLoadingURIString(gBrowser, "moz://a");
    await BrowserTestUtils.waitForLocationChange(
      gBrowser,
      `https://example.com/${path}mozprotocol.html`
    );
    ok(true, "Made it to the expected page");
  });
});
