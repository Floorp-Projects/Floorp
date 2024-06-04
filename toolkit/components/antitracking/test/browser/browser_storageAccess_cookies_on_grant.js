/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Inserts an iframe element and resolves once the iframe has loaded.
 * @param {*} browser - Browser to insert the iframe into.
 * @param {string} url - URL to load in the iframe.
 * @returns {Promise<BrowsingContext>} Promise which resolves to the iframe's
 * BrowsingContext.
 */
function insertIframeAndWaitForLoad(browser, url) {
  return SpecialPowers.spawn(browser, [url], async url => {
    let iframe = content.document.createElement("iframe");
    iframe.src = url;
    content.document.body.appendChild(iframe);
    // Wait for it to load.
    await ContentTaskUtils.waitForEvent(iframe, "load");

    return iframe.browsingContext;
  });
}

// Tests that first party cookies are available to an iframe with storage
// access, without reload, while the first party context tab is still open.
add_task(async function test_with_first_party_tab_open() {
  await BrowserTestUtils.withNewTab("https://example.org", async browser => {
    info("Set a first party cookie via `document.cookie`.");
    await SpecialPowers.spawn(browser, [], async () => {
      content.document.cookie = "foo=bar; Secure; SameSite=None; max-age=3600;";
    });

    info("Keep example.org's first party tab open.");
    await BrowserTestUtils.withNewTab("https://example.com", async browser => {
      info("Insert an iframe with example.org.");
      let iframeBC = await insertIframeAndWaitForLoad(
        browser,
        "https://example.org"
      );

      await SpecialPowers.spawn(iframeBC, [], async () => {
        ok(
          !(await content.document.hasStorageAccess()),
          "example.org should not have storage access initially."
        );

        info("Simulate user activation.");
        SpecialPowers.wrap(content.document).notifyUserGestureActivation();

        info("Request storage access.");
        await content.document.requestStorageAccess();

        ok(
          await content.document.hasStorageAccess(),
          "example.org should have storage access after request succeeded."
        );

        is(
          content.document.cookie,
          "foo=bar",
          "example.org should have access to the cookie set in the first party context previously."
        );
      });
    });
  });

  info("Cleanup.");
  Services.perms.removeAll();
  Services.cookies.removeAll();
});

// Tests that first party cookies are available to an iframe with storage
// access, without reload, after the first party context tab has been closed.
add_task(async function test_all_tabs_closed() {
  await BrowserTestUtils.withNewTab("https://example.org", async browser => {
    info("Set a first party cookie via `document.cookie`.");
    await SpecialPowers.spawn(browser, [], async () => {
      content.document.cookie = "foo=bar; Secure; SameSite=None; max-age=3600;";
    });
  });
  info(
    "Now that example.org's tab is closed, open a new tab with example.com which embeds example.org."
  );
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    info("Insert an iframe with example.org.");
    let iframeBC = await insertIframeAndWaitForLoad(
      browser,
      "https://example.org"
    );

    await SpecialPowers.spawn(iframeBC, [], async () => {
      ok(
        !(await content.document.hasStorageAccess()),
        "example.org should not have storage access initially."
      );

      content.document.cookie =
        "fooPartitioned=bar; Secure; SameSite=None; max-age=3600;";

      info("Simulate user activation.");
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();

      info("Request storage access.");
      await content.document.requestStorageAccess();

      ok(
        await content.document.hasStorageAccess(),
        "example.org should have storage access after request succeeded."
      );

      ok(
        content.document.cookie.includes("foo=bar"),
        "example.org should have access to the cookie set in the first party context previously."
      );
    });
  });

  info("Cleanup.");
  Services.perms.removeAll();
  Services.cookies.removeAll();
});

// Tests that an iframe with storage access receives cookie changes done in
// another tab in the first party context.
add_task(async function test_cookie_updates_broadcasted_to_other_tabs() {
  info("Open a new tab with example.com which embeds example.org.");
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    info("Insert an iframe with example.org.");
    let iframeBC = await insertIframeAndWaitForLoad(
      browser,
      "https://example.org"
    );

    await SpecialPowers.spawn(iframeBC, [], async () => {
      ok(
        !(await content.document.hasStorageAccess()),
        "example.org should not have storage access initially."
      );

      info("Simulate user activation.");
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();

      info("Request storage access.");
      await content.document.requestStorageAccess();

      ok(
        await content.document.hasStorageAccess(),
        "example.org should have storage access after request succeeded."
      );
    });

    info("Open a new tab with example.org to set a first party cookie.");
    await BrowserTestUtils.withNewTab("https://example.org", async browser => {
      info("Set a first party cookie via `document.cookie`");
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.cookie =
          "foo=bar; Secure; SameSite=None; max-age=3600;";
      });

      info("Make sure the cookie arrived in the example.org iframe.");
      await SpecialPowers.spawn(iframeBC, [], async () => {
        is(
          content.document.cookie,
          "foo=bar",
          "example.org should have access to the cookie set in the first party context."
        );
      });
    });

    info(
      "The first party tab has been closed. Make sure the cookie is still available in the iframe."
    );
    await SpecialPowers.spawn(iframeBC, [], async () => {
      is(
        content.document.cookie,
        "foo=bar",
        "example.org should have access to the cookie set in the first party context."
      );
    });
  });

  info("Cleanup.");
  Services.perms.removeAll();
  Services.cookies.removeAll();
});
