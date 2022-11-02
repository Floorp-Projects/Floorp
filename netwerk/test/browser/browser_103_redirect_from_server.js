Services.prefs.setBoolPref("network.early-hints.enabled", true);

const { lax_request_count_checking } = ChromeUtils.import(
  "resource://testing-common/early_hint_preload_test_helper.jsm"
);

// - testName is just there to be printed during Asserts when failing
// - crossOrigin is to determine if the redirect is to a different tld.
async function test_hint_completion_on_redirect(
  testName,
  crossOrigin,
  expectedCount
) {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&redirect=1&hinted=1&crossOrigin=${
    crossOrigin ? "1" : "0"
  }`;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function() {}
  );

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  // TODO: Switch to stricter counting method after fixing https://bugzilla.mozilla.org/show_bug.cgi?id=1753730#c11
  await lax_request_count_checking(testName, gotRequestCount, expectedCount);
}

/**
 * The two tests follow the basic same 3 steps:
 * 1. Firefox loads early_hint_asset_html.sjs?as=image&code=${httpCode}&hinted=1&crossOrigin=0
 * 2. Server returns 103 response with Link to early_hint_asset.sjs?as=image
 * 3. The server then returns a 302 with location early_hint_main_html.sjs.
 * The expected number of early hints for "test_eh_before_redirect" is 1 before the redirect occurs.
 * This test redirects to the same origin and which is why the early hint won't be canceled.
 */
add_task(async function test_eh_before_redirect() {
  await test_hint_completion_on_redirect(`test_eh_before_redirect`, false, {
    hinted: 1,
    normal: 0,
  });
});

/**
 * This test follows the same step as above with crossOrigin=1 which redirects the test to a different tld.
 * The expected number of early hints is 0 since we redirect to a different origin. The number of normal is 0 too
 * since the count isn't increased in early_hint_main_html.sjs.
 */
add_task(async function test_redirect_eh_different_tld() {
  await test_hint_completion_on_redirect(
    `test_redirect_eh_before_different_tld`,
    true,
    { hinted: 0, normal: 0 }
  );
});
