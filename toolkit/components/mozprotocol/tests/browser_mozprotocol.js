// Check that entering moz://a into the address bar directs us to a new url
add_task(function*() {
  let path = getRootDirectory(gTestPath).substring("chrome://mochitests/content/".length);
  yield SpecialPowers.pushPrefEnv({
    set: [["toolkit.mozprotocol.url", `https://example.com/${path}mozprotocol.html`]],
  });

  yield BrowserTestUtils.withNewTab("about:blank", function*() {
    gBrowser.loadURI("moz://a");
    yield BrowserTestUtils.waitForLocationChange(gBrowser,
      `https://example.com/${path}mozprotocol.html`);
    ok(true, "Made it to the expected page");
  });
});
