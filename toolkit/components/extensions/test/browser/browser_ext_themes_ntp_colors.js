"use strict";
// This test checks whether the new tab page color properties work.

function waitForAboutNewTabReady(browser, url) {
  // Stop-gap fix for https://bugzilla.mozilla.org/show_bug.cgi?id=1697196#c24
  return SpecialPowers.spawn(browser, [url], async url => {
    let doc = content.document;
    await ContentTaskUtils.waitForCondition(
      () => doc.querySelector(".outer-wrapper"),
      `Waiting for page wrapper to be initialized at ${url}`
    );
  });
}

/**
 * Test whether the selected browser has the new tab page theme applied
 * @param {Object} theme that is applied
 * @param {boolean} isBrightText whether the brighttext attribute should be set
 */
async function test_ntp_theme(theme, isBrightText) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme,
    },
  });

  let browser = gBrowser.selectedBrowser;

  let {
    originalBackground,
    originalCardBackground,
    originalColor,
  } = await SpecialPowers.spawn(browser, [], function() {
    let doc = content.document;
    ok(
      !doc.body.hasAttribute("lwt-newtab"),
      "New tab page should not have lwt-newtab attribute"
    );
    ok(
      !doc.body.hasAttribute("lwt-newtab-brighttext"),
      `New tab page should not have lwt-newtab-brighttext attribute`
    );

    return {
      originalBackground: content.getComputedStyle(doc.body).backgroundColor,
      originalCardBackground: content.getComputedStyle(
        doc.querySelector(".top-site-outer .tile")
      ).backgroundColor,
      originalColor: content.getComputedStyle(
        doc.querySelector(".outer-wrapper")
      ).color,
      // We check the value of --newtab-link-primary-color directly because the
      // elements on which it is applied are hard to test. It is most visible in
      // the "learn more" link in the Pocket section. We cannot show the Pocket
      // section since it hits the network, and the usual workarounds to change
      // its backend only work in browser/. This variable is also used in
      // the Edit Top Site modal, but showing/hiding that is very verbose and
      // would make this test almost unreadable.
      originalLinks: content
        .getComputedStyle(doc.querySelector("body"))
        .getPropertyValue("--newtab-link-primary-color"),
    };
  });

  await extension.startup();

  Services.ppmm.sharedData.flush();

  await SpecialPowers.spawn(
    browser,
    [
      {
        isBrightText,
        background: hexToCSS(theme.colors.ntp_background),
        card_background: hexToCSS(theme.colors.ntp_card_background),
        color: hexToCSS(theme.colors.ntp_text),
      },
    ],
    async function({ isBrightText, background, card_background, color }) {
      let doc = content.document;
      ok(
        doc.body.hasAttribute("lwt-newtab"),
        "New tab page should have lwt-newtab attribute"
      );
      is(
        doc.body.hasAttribute("lwt-newtab-brighttext"),
        isBrightText,
        `New tab page should${
          !isBrightText ? " not" : ""
        } have lwt-newtab-brighttext attribute`
      );

      is(
        content.getComputedStyle(doc.body).backgroundColor,
        background,
        "New tab page background should be set."
      );
      is(
        content.getComputedStyle(doc.querySelector(".top-site-outer .tile"))
          .backgroundColor,
        card_background,
        "New tab page card background should be set."
      );
      is(
        content.getComputedStyle(doc.querySelector(".outer-wrapper")).color,
        color,
        "New tab page text color should be set."
      );
    }
  );

  await extension.unload();

  Services.ppmm.sharedData.flush();

  await SpecialPowers.spawn(
    browser,
    [
      {
        originalBackground,
        originalCardBackground,
        originalColor,
      },
    ],
    function({ originalBackground, originalCardBackground, originalColor }) {
      let doc = content.document;
      ok(
        !doc.body.hasAttribute("lwt-newtab"),
        "New tab page should not have lwt-newtab attribute"
      );
      ok(
        !doc.body.hasAttribute("lwt-newtab-brighttext"),
        `New tab page should not have lwt-newtab-brighttext attribute`
      );

      is(
        content.getComputedStyle(doc.body).backgroundColor,
        originalBackground,
        "New tab page background should be reset."
      );
      is(
        content.getComputedStyle(doc.querySelector(".top-site-outer .tile"))
          .backgroundColor,
        originalCardBackground,
        "New tab page card background should be reset."
      );
      is(
        content.getComputedStyle(doc.querySelector(".outer-wrapper")).color,
        originalColor,
        "New tab page text color should be reset."
      );
    }
  );
}

add_task(async function test_support_ntp_colors() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // BrowserTestUtils.withNewTab waits for about:newtab to load
      // so we disable preloading before running the test.
      ["browser.newtab.preload", false],
      // Force prefers-color-scheme to "system", as otherwise it is derived
      // from the newtab background colors, but we hard-code the light styles
      // on this test.
      ["layout.css.prefers-color-scheme.content-override", 2],
    ],
  });
  NewTabPagePreloading.removePreloadedBrowser(window);
  for (let url of ["about:newtab", "about:home"]) {
    info("Opening url: " + url);
    await BrowserTestUtils.withNewTab({ gBrowser, url }, async browser => {
      await waitForAboutNewTabReady(browser, url);
      await test_ntp_theme(
        {
          colors: {
            frame: ACCENT_COLOR,
            tab_background_text: TEXT_COLOR,
            ntp_background: "#add8e6",
            ntp_card_background: "#ffffff",
            ntp_text: "#00008b",
          },
        },
        false,
        url
      );

      await test_ntp_theme(
        {
          colors: {
            frame: ACCENT_COLOR,
            tab_background_text: TEXT_COLOR,
            ntp_background: "#00008b",
            ntp_card_background: "#000000",
            ntp_text: "#add8e6",
          },
        },
        true,
        url
      );
    });
  }
});
