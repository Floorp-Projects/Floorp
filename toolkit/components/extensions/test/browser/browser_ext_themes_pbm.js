/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/**
 * Tests that we apply dark theme variants to PBM windows where applicable.
 */

const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);
const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

const IS_LINUX = AppConstants.platform == "linux";

const LIGHT_THEME_ID = "firefox-compact-light@mozilla.org";
const DARK_THEME_ID = "firefox-compact-dark@mozilla.org";

// This tests opens many chrome windows which is slow on debug builds.
requestLongerTimeout(2);

/**
 * Test a window's theme color scheme.
 * @param {*} options - Test options.
 * @param {Window} options.win - Window object to test.
 * @param {boolean} options.colorScheme - Whether expected chrome color scheme
 * is dark (true) or light (false).
 * @param {boolean} options.expectLWTAttributes - Whether the window  should
 * have the LWT attributes set matching the color scheme.
 * @param {boolean} options.expectDefaultDarkAttribute - Whether the window
 * should have the "lwt-default-theme-in-dark-mode" attribute.
 */
async function testWindowColorScheme({
  win,
  expectDark,
  expectLWTAttributes,
  expectDefaultDarkAttribute,
}) {
  let docEl = win.document.documentElement;

  is(
    docEl.hasAttribute("lwt-default-theme-in-dark-mode"),
    expectDefaultDarkAttribute,
    `Window should${
      expectDefaultDarkAttribute ? "" : " not"
    } have lwt-default-theme-in-dark-mode attribute.`
  );

  if (expectLWTAttributes) {
    ok(docEl.hasAttribute("lwtheme"), "Window should have LWT attribute.");
    is(
      docEl.getAttribute("lwthemetextcolor"),
      expectDark ? "bright" : "dark",
      "LWT text color attribute should be set."
    );
  } else {
    ok(!docEl.hasAttribute("lwtheme"), "Window should not have LWT attribute.");
    ok(
      !docEl.hasAttribute("lwthemetextcolor"),
      "LWT text color attribute should not be set."
    );
  }
}

/**
 * Match the prefers-color-scheme media query and return the results.
 * @param {Object} options
 * @param {Window} options.win - If chrome=true, window to test, otherwise
 * parent window of the content window to test.
 * @param {boolean} options.chrome - If true the media queries will be matched
 * against the supplied chrome window. Otherwise they will be matched against
 * the content window.
 * @returns  {Promise<{light: boolean, dark: boolean}>} - Resolves with an
 * object of the media query results.
 */
function getPrefersColorSchemeInfo({ win, chrome = false }) {
  let fn = async windowObj => {
    // If called in the parent, we use the supplied win object. Otherwise use
    // the content window global.
    let win = windowObj || content;

    // LookAndFeel updates are async.
    await new Promise(resolve => {
      win.requestAnimationFrame(() => win.requestAnimationFrame(resolve));
    });
    return {
      light: win.matchMedia("(prefers-color-scheme: light)").matches,
      dark: win.matchMedia("(prefers-color-scheme: dark)").matches,
    };
  };

  if (chrome) {
    return fn(win);
  }

  return SpecialPowers.spawn(win.gBrowser.selectedBrowser, [], fn);
}

add_task(async function setup() {
  // Set system theme to light to ensure consistency across test machines.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.theme.dark-private-windows", true],
      ["ui.systemUsesDarkTheme", 0],
    ],
  });
  // Ensure the built-in themes are initialized.
  await BuiltInThemes.ensureBuiltInThemes();

  // The previous test, browser_ext_themes_ntp_colors.js has side effects.
  // Switch to a theme, then switch back to the default theme to reach a
  // consistent themeData state. Without this, themeData in
  // LightWeightConsumer#_update does not contain darkTheme data and PBM windows
  // don't get themed correctly.
  let lightTheme = await AddonManager.getAddonByID(LIGHT_THEME_ID);
  await lightTheme.enable();
  await lightTheme.disable();
});

// For the default theme with light color scheme, private browsing windows
// should be themed dark.
// The PBM window's content should not be themed dark.
add_task(async function test_default_theme_light() {
  info("Normal browsing window should not be in dark mode.");
  await testWindowColorScheme({
    win: window,
    expectDark: false,
    expectLWTAttributes: false,
    expectDefaultDarkAttribute: false,
  });

  let windowB = await BrowserTestUtils.openNewBrowserWindow();

  info("Additional normal browsing window should not be in dark mode.");
  await testWindowColorScheme({
    win: windowB,
    expectDark: false,
    expectLWTAttributes: false,
    expectDefaultDarkAttribute: false,
  });

  let pbmWindowA = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info("Private browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: pbmWindowA,
    expectDark: true,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: true,
  });

  let prefersColorScheme = await getPrefersColorSchemeInfo({ win: pbmWindowA });
  ok(
    prefersColorScheme.light && !prefersColorScheme.dark,
    "Content of dark themed PBM window should still be themed light"
  );

  let pbmWindowB = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  info("Additional private browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: pbmWindowB,
    expectDark: true,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: true,
  });

  await BrowserTestUtils.closeWindow(windowB);
  await BrowserTestUtils.closeWindow(pbmWindowA);
  await BrowserTestUtils.closeWindow(pbmWindowB);
});

// For the default theme with dark color scheme, normal and private browsing
// windows should be themed dark.
add_task(async function test_default_theme_dark() {
  // Set the system theme to dark. The default theme will follow this color
  // scheme.
  await SpecialPowers.pushPrefEnv({ set: [["ui.systemUsesDarkTheme", 1]] });

  info("Normal browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: window,
    expectDark: true,
    expectLWTAttributes: !IS_LINUX,
    expectDefaultDarkAttribute: !IS_LINUX,
  });

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info("Private browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: pbmWindow,
    expectDark: true,
    expectLWTAttributes: !IS_LINUX,
    expectDefaultDarkAttribute: !IS_LINUX,
  });

  await BrowserTestUtils.closeWindow(pbmWindow);

  await SpecialPowers.popPrefEnv();
});

// For the light theme both normal and private browsing windows should have a
// bright color scheme applied.
add_task(async function test_light_theme_builtin() {
  let lightTheme = await AddonManager.getAddonByID(LIGHT_THEME_ID);
  await lightTheme.enable();

  info("Normal browsing window should not be in dark mode.");
  await testWindowColorScheme({
    win: window,
    expectDark: false,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: false,
  });

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  info("Private browsing window should not be in dark mode.");
  await testWindowColorScheme({
    win: pbmWindow,
    expectDark: false,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: false,
  });

  await BrowserTestUtils.closeWindow(pbmWindow);
  await lightTheme.disable();
});

// For the dark theme both normal and private browsing should have a dark color
// scheme applied.
add_task(async function test_dark_theme_builtin() {
  let darkTheme = await AddonManager.getAddonByID(DARK_THEME_ID);
  await darkTheme.enable();

  info("Normal browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: window,
    expectDark: true,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: false,
  });

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info("Private browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: pbmWindow,
    expectDark: true,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: false,
  });

  await BrowserTestUtils.closeWindow(pbmWindow);
  await darkTheme.disable();
});

// When switching between default, light and dark theme the private browsing
// window color scheme should update accordingly.
add_task(async function test_theme_switch_updates_existing_pbm_win() {
  let windowB = await BrowserTestUtils.openNewBrowserWindow();
  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info("Normal browsing window should not be in dark mode.");
  await testWindowColorScheme({
    win: window,
    expectDark: false,
    expectLWTAttributes: false,
    expectDefaultDarkAttribute: false,
  });

  info("Private browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: pbmWindow,
    expectDark: true,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: true,
  });

  info("Enabling light theme.");
  let lightTheme = await AddonManager.getAddonByID(LIGHT_THEME_ID);
  await lightTheme.enable();

  info("Normal browsing window should not be in dark mode.");
  await testWindowColorScheme({
    win: window,
    expectDark: false,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: false,
  });

  info("Private browsing window should not be in dark mode.");
  await testWindowColorScheme({
    win: pbmWindow,
    expectDark: false,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: false,
  });

  await lightTheme.disable();

  info("Enabling dark theme.");
  let darkTheme = await AddonManager.getAddonByID(DARK_THEME_ID);
  await darkTheme.enable();

  info("Normal browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: window,
    expectDark: true,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: false,
  });

  info("Private browsing window should be in dark mode.");
  await testWindowColorScheme({
    win: pbmWindow,
    expectDark: true,
    expectLWTAttributes: true,
    expectDefaultDarkAttribute: false,
  });

  await darkTheme.disable();

  await BrowserTestUtils.closeWindow(windowB);
  await BrowserTestUtils.closeWindow(pbmWindow);
});

// pageInfo windows should inherit the PBM window dark theme.
add_task(async function test_pbm_dark_page_info() {
  for (let isPBM of [false, true]) {
    let win = await BrowserTestUtils.openNewBrowserWindow({
      private: isPBM,
    });
    let windowTypeStr = isPBM ? "private" : "normal";

    info(`Opening pageInfo from ${windowTypeStr} browsing.`);

    await BrowserTestUtils.withNewTab(
      { gBrowser: win.gBrowser, url: "https://example.com" },
      async () => {
        let pageInfo = win.BrowserPageInfo(null, "securityTab");
        await BrowserTestUtils.waitForEvent(pageInfo, "page-info-init");

        let prefersColorScheme = await getPrefersColorSchemeInfo({
          win: pageInfo,
          chrome: true,
        });
        if (isPBM) {
          ok(
            !prefersColorScheme.light && prefersColorScheme.dark,
            "pageInfo from private window should be themed dark."
          );
        } else {
          ok(
            prefersColorScheme.light && !prefersColorScheme.dark,
            "pageInfo from normal window should be themed light."
          );
        }

        pageInfo.close();
      }
    );

    await BrowserTestUtils.closeWindow(win);
  }
});

// Prompts should inherit the PBM window dark theme.
add_task(async function test_pbm_dark_prompts() {
  const { MODAL_TYPE_TAB, MODAL_TYPE_CONTENT } = Services.prompt;

  for (let isPBM of [false, true]) {
    let win = await BrowserTestUtils.openNewBrowserWindow({
      private: isPBM,
    });

    // TODO: Once Bug 1751953 has been fixed, we can also test MODAL_TYPE_WINDOW
    // here.
    for (let modalType of [MODAL_TYPE_TAB, MODAL_TYPE_CONTENT]) {
      let windowTypeStr = isPBM ? "private" : "normal";
      let modalTypeStr = modalType == MODAL_TYPE_TAB ? "tab" : "content";

      info(`Opening ${modalTypeStr} prompt from ${windowTypeStr} browsing.`);

      let openPromise = PromptTestUtils.waitForPrompt(
        win.gBrowser.selectedBrowser,
        {
          modalType,
          promptType: "alert",
        }
      );
      let promptPromise = Services.prompt.asyncAlert(
        win.gBrowser.selectedBrowser.browsingContext,
        modalType,
        "Hello",
        "Hello, world!"
      );

      let dialog = await openPromise;

      let prefersColorScheme = await getPrefersColorSchemeInfo({
        win: dialog.ui.prompt,
        chrome: true,
      });

      if (isPBM) {
        ok(
          !prefersColorScheme.light && prefersColorScheme.dark,
          "Prompt from private window should be themed dark."
        );
      } else {
        ok(
          prefersColorScheme.light && !prefersColorScheme.dark,
          "Prompt from normal window should be themed light."
        );
      }

      await PromptTestUtils.handlePrompt(dialog);
      await promptPromise;
    }

    await BrowserTestUtils.closeWindow(win);
  }
});
