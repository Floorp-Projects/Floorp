/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

const LIGHT_SCHEME_BG = "rgb(255, 255, 255)";
const LIGHT_SCHEME_FG = "rgb(0, 0, 0)";

// "browser.display.background_color.dark" pref value ("#1C1B22") maps to:
const DARK_SCHEME_BG = "rgb(28, 27, 34)";
const DARK_SCHEME_FG = "rgb(251, 251, 254)";

async function getColorsForOptionsUI({ browser_style, open_in_tab }) {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      options_ui: {
        browser_style,
        page: "options.html",
        open_in_tab,
      },
    },
    background() {
      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq("openOptionsPage", msg, "expect openOptionsPage");
        browser.runtime.openOptionsPage();
      });
    },
    files: {
      "options.html": `<style>:root { color-scheme: dark light; }</style>
        <script src="options.js"></script>`,
      "options.js": () => {
        window.onload = () => {
          browser.test.sendMessage("options_ui_opened");
        };
      },
    },
  });

  await extension.startup();
  extension.sendMessage("openOptionsPage");
  await extension.awaitMessage("options_ui_opened");

  const tab = gBrowser.selectedTab;
  let optionsBrowser;
  if (open_in_tab) {
    optionsBrowser = tab.linkedBrowser;
    is(
      optionsBrowser.currentURI.spec,
      `moz-extension://${extension.uuid}/options.html`,
      "With open_in_tab=true, should open options.html in tab"
    );
  } else {
    // When not opening in a new tab, the inline options page is used.
    is(
      tab.linkedBrowser.currentURI.spec,
      "about:addons",
      "Without open_in_tab, should open about:addons"
    );
    optionsBrowser = tab.linkedBrowser.contentDocument.getElementById(
      "addon-inline-options"
    );
    is(
      optionsBrowser.currentURI.spec,
      `moz-extension://${extension.uuid}/options.html`,
      "Found options.html in inline options browser"
    );
  }

  let colors = await SpecialPowers.spawn(optionsBrowser, [], () => {
    let style = content.getComputedStyle(content.document.body);
    // Note: cannot use style.backgroundColor because it defaults to
    // "transparent" (aka rgba(0, 0, 0, 0)) which is meaningless.
    // So we have to use windowUtils.canvasBackgroundColor instead.
    return {
      bgColor: content.windowUtils.canvasBackgroundColor,
      fgColor: style.color,
    };
  });

  if (colors.bgColor === "rgba(0, 0, 0, 0)") {
    // windowUtils.canvasBackgroundColor may still report a "transparent"
    // background color when the options page is rendered inline in a <browser>
    // at about:addons. In that case, the background color of the container
    // element (i.e. the <browser>) is used to render the contents.
    Assert.ok(!open_in_tab, "Background only transparent without open_in_tab");
    let style = optionsBrowser.ownerGlobal.getComputedStyle(optionsBrowser);
    colors.bgColor = style.backgroundColor;
  }

  BrowserTestUtils.removeTab(tab);

  await extension.unload();
  return colors;
}

add_setup(async () => {
  // The test calls openOptionsPage, which may end up re-using an existing blank
  // tab. Upon closing that, the browser window of the test would close and the
  // test would get stuck. To avoid that, make sure that there is a dummy tab
  // around that keeps the window open.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "data:,");
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function options_ui_open_in_tab_light() {
  await SpecialPowers.pushPrefEnv({ set: [["ui.systemUsesDarkTheme", 0]] });
  // Note: browser_style:true should be no-op when open_in_tab:true.
  // Therefore the result should be equivalent to the color of a normal web
  // page, instead of options_ui_inline_light.
  Assert.deepEqual(
    await getColorsForOptionsUI({ browser_style: true, open_in_tab: true }),
    { bgColor: LIGHT_SCHEME_BG, fgColor: LIGHT_SCHEME_FG }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function options_ui_open_in_tab_dark() {
  await SpecialPowers.pushPrefEnv({ set: [["ui.systemUsesDarkTheme", 1]] });
  // Note: browser_style:true should be no-op when open_in_tab:true.
  // Therefore the result should be equivalent to the color of a normal web
  // page, instead of options_ui_inline_dark.
  Assert.deepEqual(
    await getColorsForOptionsUI({ browser_style: true, open_in_tab: true }),
    { bgColor: DARK_SCHEME_BG, fgColor: DARK_SCHEME_FG }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function options_ui_light() {
  await SpecialPowers.pushPrefEnv({ set: [["ui.systemUsesDarkTheme", 0]] });
  Assert.deepEqual(
    await getColorsForOptionsUI({ browser_style: false, open_in_tab: false }),
    { bgColor: LIGHT_SCHEME_BG, fgColor: LIGHT_SCHEME_FG }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function options_ui_dark() {
  await SpecialPowers.pushPrefEnv({ set: [["ui.systemUsesDarkTheme", 1]] });
  Assert.deepEqual(
    await getColorsForOptionsUI({ browser_style: false, open_in_tab: false }),
    { bgColor: DARK_SCHEME_BG, fgColor: DARK_SCHEME_FG }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function options_ui_browser_style_true_light() {
  await SpecialPowers.pushPrefEnv({ set: [["ui.systemUsesDarkTheme", 0]] });
  Assert.deepEqual(
    await getColorsForOptionsUI({ browser_style: true, open_in_tab: false }),
    // rgb(34, 36, 38) = color: #222426 from extension.css
    { bgColor: LIGHT_SCHEME_BG, fgColor: "rgb(34, 36, 38)" }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function options_ui_browser_style_true_dark() {
  await SpecialPowers.pushPrefEnv({ set: [["ui.systemUsesDarkTheme", 1]] });
  Assert.deepEqual(
    await getColorsForOptionsUI({ browser_style: true, open_in_tab: false }),
    { bgColor: DARK_SCHEME_BG, fgColor: DARK_SCHEME_FG }
  );
  await SpecialPowers.popPrefEnv();
});
