function checkAllowListPrincipal(
  browser,
  type,
  origin = browser.contentPrincipal.origin
) {
  let principal = browser.contentBlockingAllowListPrincipal;
  ok(principal, "Principal is set");

  if (type == "content") {
    ok(principal.isContentPrincipal, "Is content principal");

    ok(
      principal.schemeIs("https"),
      "allowlist content principal must have https scheme"
    );
  } else if (type == "system") {
    ok(principal.isSystemPrincipal, "Is system principal");
  } else {
    throw new Error("Unexpected principal type");
  }

  is(origin, principal.origin, "Correct origin");
}

/**
 * Test that we get the a allow list principal which matches the content
 * principal for an https site.
 */
add_task(async test_contentPrincipalHTTPS => {
  await BrowserTestUtils.withNewTab("https://example.com", browser => {
    checkAllowListPrincipal(browser, "content");
  });
});

/**
 * Tests that we the scheme of the allowlist principal is HTTPS, even though the
 * site is loaded via HTTP.
 */
add_task(async test_contentPrincipalHTTP => {
  await BrowserTestUtils.withNewTab("http://example.net", browser => {
    ok(
      browser.contentPrincipal.isContentPrincipal,
      "Should have content principal"
    );
    checkAllowListPrincipal(browser, "content", "https://example.net");
  });
});

/**
 * Tests that the allow list principal is a system principal for the preferences
 * about site.
 */
add_task(async test_systemPrincipal => {
  await BrowserTestUtils.withNewTab("about:preferences", browser => {
    ok(
      browser.contentPrincipal.isSystemPrincipal,
      "Should have system principal"
    );
    checkAllowListPrincipal(browser, "system");
  });
});

/**
 * Open a site in private browsing and test that the allow list principal sets
 * the same origin attributes as the content principal.
 */
add_task(async test_privateBrowsing => {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab = BrowserTestUtils.addTab(win.gBrowser, "https://example.com");
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.browserLoaded(browser);

  ok(
    browser.contentPrincipal.isContentPrincipal,
    "Should have content principal"
  );
  checkAllowListPrincipal(browser, "content");

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests that we get a valid content principal for top level sandboxed pages,
 * and not the document principal which is a null principal.
 */
add_task(async test_TopLevelSandbox => {
  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/components/antitracking/test/browser/sandboxed.html",
    browser => {
      ok(
        browser.contentPrincipal.isNullPrincipal,
        "Top level sandboxed page should have null principal"
      );
      checkAllowListPrincipal(browser, "content", "https://example.com");
    }
  );
});
