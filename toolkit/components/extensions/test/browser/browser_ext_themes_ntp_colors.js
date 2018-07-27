"use strict";

// This test checks whether the new tab page color properties work.

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
    originalColor,
  } = await ContentTask.spawn(browser, {}, function() {
    let doc = content.document;
    ok(!doc.body.hasAttribute("lwt-newtab"),
       "New tab page should not have lwt-newtab attribute");
    ok(!doc.body.hasAttribute("lwt-newtab-brighttext"),
       `New tab page should not have lwt-newtab-brighttext attribute`);

    return {
      originalBackground: content.getComputedStyle(doc.body).backgroundColor,
      originalColor: content.getComputedStyle(doc.querySelector(".outer-wrapper")).color,
    };
  });

  await extension.startup();

  Services.ppmm.sharedData.flush();

  await ContentTask.spawn(browser, {
    isBrightText,
    background: hexToCSS(theme.colors.ntp_background),
    color: hexToCSS(theme.colors.ntp_text),
  }, function({isBrightText, background, color}) {
    let doc = content.document;
    ok(doc.body.hasAttribute("lwt-newtab"),
       "New tab page should have lwt-newtab attribute");
    is(doc.body.hasAttribute("lwt-newtab-brighttext"), isBrightText,
       `New tab page should${!isBrightText ? " not" : ""} have lwt-newtab-brighttext attribute`);

    is(content.getComputedStyle(doc.body).backgroundColor, background,
       "New tab page background should be set.");
    is(content.getComputedStyle(doc.querySelector(".outer-wrapper")).color, color,
       "New tab page text color should be set.");
  });

  await extension.unload();

  Services.ppmm.sharedData.flush();

  await ContentTask.spawn(browser, {
    originalBackground,
    originalColor,
  }, function({originalBackground, originalColor}) {
    let doc = content.document;
    ok(!doc.body.hasAttribute("lwt-newtab"),
       "New tab page should not have lwt-newtab attribute");
    ok(!doc.body.hasAttribute("lwt-newtab-brighttext"),
       `New tab page should not have lwt-newtab-brighttext attribute`);

    is(content.getComputedStyle(doc.body).backgroundColor, originalBackground,
       "New tab page background should be reset.");
    is(content.getComputedStyle(doc.querySelector(".outer-wrapper")).color, originalColor,
       "New tab page text color should be reset.");
  });
}

add_task(async function test_support_ntp_colors() {
  // BrowserTestUtils.withNewTab waits for about:newtab to load
  // so we disable preloading before running the test.
  SpecialPowers.setBoolPref("browser.newtab.preload", false);
  registerCleanupFunction(() => {
    SpecialPowers.clearUserPref("browser.newtab.preload");
  });
  gBrowser.removePreloadedBrowser();
  for (let url of ["about:newtab", "about:home", "about:welcome"]) {
    info("Opening url: " + url);
    await BrowserTestUtils.withNewTab({gBrowser, url}, async browser => {
      await test_ntp_theme({
        colors: {
          accentcolor: ACCENT_COLOR,
          textcolor: TEXT_COLOR,
          ntp_background: "#add8e6",
          ntp_text: "#00008b",
        },
      }, false, url);

      await test_ntp_theme({
        colors: {
          accentcolor: ACCENT_COLOR,
          textcolor: TEXT_COLOR,
          ntp_background: "#00008b",
          ntp_text: "#add8e6",
        },
      }, true, url);
    });
  }
});
