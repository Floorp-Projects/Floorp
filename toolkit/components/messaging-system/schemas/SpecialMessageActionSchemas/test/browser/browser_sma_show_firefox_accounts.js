/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Note: "identity.fxaccounts.remote.root" is set to https://example.com in browser.ini
add_task(async function test_SHOW_FIREFOX_ACCOUNTS() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let loaded = BrowserTestUtils.browserLoaded(browser);
    await SMATestUtils.executeAndValidateAction({
      type: "SHOW_FIREFOX_ACCOUNTS",
      data: { entrypoint: "snippets" },
    });
    Assert.equal(
      await loaded,
      "https://example.com/?context=fx_desktop_v3&entrypoint=snippets&action=email&service=sync",
      "should load fxa with endpoint=snippets"
    );

    // Open a URL
    loaded = BrowserTestUtils.browserLoaded(browser);
    await SMATestUtils.executeAndValidateAction({
      type: "SHOW_FIREFOX_ACCOUNTS",
      data: { entrypoint: "aboutwelcome" },
    });

    Assert.equal(
      await loaded,
      "https://example.com/?context=fx_desktop_v3&entrypoint=aboutwelcome&action=email&service=sync",
      "should load fxa with a custom endpoint"
    );
  });
});
