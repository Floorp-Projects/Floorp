"use strict";

// This test checks whether the new tab page color properties work per-window.

/**
 * Test whether a given browser has the new tab page theme applied
 * @param {Object} browser to test against
 * @param {Object} theme that is applied
 * @param {boolean} isBrightText whether the brighttext attribute should be set
 * @returns {Promise} The task as a promise
 */
function test_ntp_theme(browser, theme, isBrightText) {
  Services.ppmm.sharedData.flush();
  return ContentTask.spawn(browser, {
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
}

/**
 * Test whether a given browser has the default theme applied
 * @param {Object} browser to test against
 * @returns {Promise} The task as a promise
 */
function test_ntp_default_theme(browser) {
  Services.ppmm.sharedData.flush();
  return ContentTask.spawn(browser, {
    background: hexToCSS("#F9F9FA"),
    color: hexToCSS("#0C0C0D"),
  }, function({background, color}) {
    let doc = content.document;
    ok(!doc.body.hasAttribute("lwt-newtab"),
       "New tab page should not have lwt-newtab attribute");
    ok(!doc.body.hasAttribute("lwt-newtab-brighttext"),
       `New tab page should not have lwt-newtab-brighttext attribute`);

    is(content.getComputedStyle(doc.body).backgroundColor, background,
       "New tab page background should be reset.");
    is(content.getComputedStyle(doc.querySelector(".outer-wrapper")).color, color,
       "New tab page text color should be reset.");
  });
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

      function createWindow() {
        return new Promise(resolve => {
          let listener = win => {
            browser.windows.onCreated.removeListener(listener);
            resolve(win);
          };
          browser.windows.onCreated.addListener(listener);
          browser.windows.create();
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
        browser.test.sendMessage("check-window", {theme, isBrightText, winId});
        await windowChecked;
      }

      const darkTextTheme = {
        colors: {
          frame: "#add8e6",
          tab_background_text: "#000",
          ntp_background: "#add8e6",
          ntp_text: "#000",
        },
      };

      const brightTextTheme = {
        colors: {
          frame: "#00008b",
          tab_background_text: "#add8e6",
          ntp_background: "#00008b",
          ntp_text: "#add8e6",
        },
      };

      let {id: winId} = await browser.windows.getCurrent();
      let {id: secondWinId} = await createWindow();

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

  extension.onMessage("check-window", async ({theme, isBrightText, winId}) => {
    let win = Services.wm.getOuterWindowWithId(winId);
    win.NewTabPagePreloading.removePreloadedBrowser(win);
    for (let url of ["about:newtab", "about:home", "about:welcome"]) {
      info("Opening url: " + url);
      await BrowserTestUtils.withNewTab({gBrowser: win.gBrowser, url}, async browser => {
        if (theme) {
          await test_ntp_theme(browser, theme, isBrightText);
        } else {
          await test_ntp_default_theme(browser);
        }
      });
    }
    extension.sendMessage("checked-window");
  });

  // BrowserTestUtils.withNewTab waits for about:newtab to load
  // so we disable preloading before running the test.
  SpecialPowers.setBoolPref("browser.newtab.preload", false);
  registerCleanupFunction(() => {
    SpecialPowers.clearUserPref("browser.newtab.preload");
  });

  await extension.startup();
  await extension.awaitFinish("perwindow-ntp-theme");
  await extension.unload();
});
