"use strict";

// This test checks whether the new tab page color properties work per-window.

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
 * Test whether a given browser has the new tab page theme applied
 * @param {Object} browser to test against
 * @param {Object} theme that is applied
 * @param {boolean} isBrightText whether the brighttext attribute should be set
 * @returns {Promise} The task as a promise
 */
function test_ntp_theme(browser, theme, isBrightText) {
  Services.ppmm.sharedData.flush();
  return SpecialPowers.spawn(
    browser,
    [
      {
        isBrightText,
        background: hexToCSS(theme.colors.ntp_background),
        card_background: hexToCSS(theme.colors.ntp_card_background),
        color: hexToCSS(theme.colors.ntp_text),
      },
    ],
    function({ isBrightText, background, card_background, color }) {
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
}

/**
 * Test whether a given browser has the default theme applied
 * @param {Object} browser to test against
 * @param {string} url being tested
 * @returns {Promise} The task as a promise
 */
function test_ntp_default_theme(browser, url) {
  Services.ppmm.sharedData.flush();
  return SpecialPowers.spawn(
    browser,
    [
      {
        background: hexToCSS("#F9F9FB"),
        color: hexToCSS("#15141A"),
      },
    ],
    function({ background, color }) {
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
        background,
        "New tab page background should be reset."
      );
      is(
        content.getComputedStyle(doc.querySelector(".outer-wrapper")).color,
        color,
        "New tab page text color should be reset."
      );
    }
  );
}

add_task(async function test_per_window_ntp_theme() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
    },
    async background() {
      function promiseWindowChecked() {
        return new Promise(resolve => {
          let listener = msg => {
            if (msg == "checked-window") {
              browser.test.onMessage.removeListener(listener);
              resolve();
            }
          };
          browser.test.onMessage.addListener(listener);
        });
      }

      function removeWindow(winId) {
        return new Promise(resolve => {
          let listener = removedWinId => {
            if (removedWinId == winId) {
              browser.windows.onRemoved.removeListener(listener);
              resolve();
            }
          };
          browser.windows.onRemoved.addListener(listener);
          browser.windows.remove(winId);
        });
      }

      async function checkWindow(theme, isBrightText, winId) {
        let windowChecked = promiseWindowChecked();
        browser.test.sendMessage("check-window", {
          theme,
          isBrightText,
          winId,
        });
        await windowChecked;
      }

      const darkTextTheme = {
        colors: {
          frame: "#add8e6",
          tab_background_text: "#000",
          ntp_background: "#add8e6",
          ntp_card_background: "#ff0000",
          ntp_text: "#000",
        },
      };

      const brightTextTheme = {
        colors: {
          frame: "#00008b",
          tab_background_text: "#add8e6",
          ntp_background: "#00008b",
          ntp_card_background: "#00ff00",
          ntp_text: "#add8e6",
        },
      };

      let { id: winId } = await browser.windows.getCurrent();
      // We are opening about:blank instead of the default homepage,
      // because using the default homepage results in intermittent
      // test failures on debug builds due to browser window leaks.
      // A side effect of testing on about:blank is that
      // test_ntp_default_theme cannot test properties used only on
      // about:newtab, like ntp_card_background.
      let { id: secondWinId } = await browser.windows.create({
        url: "about:blank",
      });

      browser.test.log("Test that single window update works");
      await browser.theme.update(winId, darkTextTheme);
      await checkWindow(darkTextTheme, false, winId);
      await checkWindow(null, false, secondWinId);

      browser.test.log("Test that applying different themes on both windows");
      await browser.theme.update(secondWinId, brightTextTheme);
      await checkWindow(darkTextTheme, false, winId);
      await checkWindow(brightTextTheme, true, secondWinId);

      browser.test.log("Test resetting the theme on one window");
      await browser.theme.reset(winId);
      await checkWindow(null, false, winId);
      await checkWindow(brightTextTheme, true, secondWinId);

      await removeWindow(secondWinId);
      await checkWindow(null, false, winId);
      browser.test.notifyPass("perwindow-ntp-theme");
    },
  });

  extension.onMessage(
    "check-window",
    async ({ theme, isBrightText, winId }) => {
      let win = Services.wm.getOuterWindowWithId(winId);
      win.NewTabPagePreloading.removePreloadedBrowser(win);
      // These pages were initially chosen because LightweightThemeChild.jsm
      // treats them specially.
      for (let url of ["about:newtab", "about:home"]) {
        info("Opening url: " + url);
        await BrowserTestUtils.withNewTab(
          { gBrowser: win.gBrowser, url },
          async browser => {
            await waitForAboutNewTabReady(browser, url);
            if (theme) {
              await test_ntp_theme(browser, theme, isBrightText);
            } else {
              await test_ntp_default_theme(browser, url);
            }
          }
        );
      }
      extension.sendMessage("checked-window");
    }
  );

  // BrowserTestUtils.withNewTab waits for about:newtab to load
  // so we disable preloading before running the test.
  await SpecialPowers.setBoolPref("browser.newtab.preload", false);
  registerCleanupFunction(() => {
    SpecialPowers.clearUserPref("browser.newtab.preload");
  });

  await extension.startup();
  await extension.awaitFinish("perwindow-ntp-theme");
  await extension.unload();
});
