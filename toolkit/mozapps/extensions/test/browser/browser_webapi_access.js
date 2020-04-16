/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function check_frame_availability(browser) {
  return check_availability(browser.browsingContext.children[0]);
}

function check_availability(browser) {
  return SpecialPowers.spawn(browser, [], async function() {
    return content.document.getElementById("result").textContent == "true";
  });
}

// Test that initially the API isn't available in the test domain
add_task(async function test_not_available() {
  await BrowserTestUtils.withNewTab(
    `${SECURE_TESTROOT}webapi_checkavailable.html`,
    async function test_not_available(browser) {
      let available = await check_availability(browser);
      ok(!available, "API should not be available.");
    }
  );
});

// Test that with testing on the API is available in the test domain
add_task(async function test_available() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webapi.testing", true]],
  });

  await BrowserTestUtils.withNewTab(
    `${SECURE_TESTROOT}webapi_checkavailable.html`,
    async function test_not_available(browser) {
      let available = await check_availability(browser);
      ok(available, "API should be available.");
    }
  );
});

// Test that the API is not available in a bad domain
add_task(async function test_bad_domain() {
  await BrowserTestUtils.withNewTab(
    `${SECURE_TESTROOT2}webapi_checkavailable.html`,
    async function test_not_available(browser) {
      let available = await check_availability(browser);
      ok(!available, "API should not be available.");
    }
  );
});

// Test that the API is only available in https sites
add_task(async function test_not_available_http() {
  await BrowserTestUtils.withNewTab(
    `${TESTROOT}webapi_checkavailable.html`,
    async function test_not_available(browser) {
      let available = await check_availability(browser);
      ok(!available, "API should not be available.");
    }
  );
});

// Test that the API is available when in a frame of the test domain
add_task(async function test_available_framed() {
  await BrowserTestUtils.withNewTab(
    `${SECURE_TESTROOT}webapi_checkframed.html`,
    async function test_available(browser) {
      let available = await check_frame_availability(browser);
      ok(available, "API should be available.");
    }
  );
});

// Test that if the external frame is http then the inner frame doesn't have
// the API
add_task(async function test_not_available_http_framed() {
  await BrowserTestUtils.withNewTab(
    `${TESTROOT}webapi_checkframed.html`,
    async function test_not_available(browser) {
      let available = await check_frame_availability(browser);
      ok(!available, "API should not be available.");
    }
  );
});

// Test that if the external frame is a bad domain then the inner frame doesn't
// have the API
add_task(async function test_not_available_framed() {
  await BrowserTestUtils.withNewTab(
    `${SECURE_TESTROOT2}webapi_checkframed.html`,
    async function test_not_available(browser) {
      let available = await check_frame_availability(browser);
      ok(!available, "API should not be available.");
    }
  );
});

// Test that a window navigated to a bad domain doesn't allow access to the API
add_task(async function test_navigated_window() {
  await BrowserTestUtils.withNewTab(
    `${SECURE_TESTROOT2}webapi_checknavigatedwindow.html`,
    async function test_available(browser) {
      let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

      await SpecialPowers.spawn(browser, [], async function() {
        await content.wrappedJSObject.openWindow();
      });

      // Should be a new tab open
      let tab = await tabPromise;
      let loadPromise = BrowserTestUtils.browserLoaded(
        gBrowser.getBrowserForTab(tab)
      );

      SpecialPowers.spawn(browser, [], async function() {
        content.wrappedJSObject.navigate();
      });

      await loadPromise;

      let available = await SpecialPowers.spawn(browser, [], async function() {
        return content.wrappedJSObject.check();
      });

      ok(!available, "API should not be available.");

      gBrowser.removeTab(tab);
    }
  );
});

// Check that if a page is embedded in a chrome content UI that it can still
// access the API.
add_task(async function test_chrome_frame() {
  SpecialPowers.pushPrefEnv({
    set: [["security.allow_unsafe_parent_loads", true]],
  });

  await BrowserTestUtils.withNewTab(
    `${CHROMEROOT}webapi_checkchromeframe.xhtml`,
    async function test_available(browser) {
      let available = await check_frame_availability(browser);
      ok(available, "API should be available.");
    }
  );
});
