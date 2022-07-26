"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// popup properties are applied correctly to the autocomplete bar.
const POPUP_COLOR_DARK = "#00A400";
const POPUP_COLOR_BRIGHT = "#85A4FF";
const POPUP_TEXT_COLOR_DARK = "#000000";
const POPUP_TEXT_COLOR_BRIGHT = "#ffffff";
const POPUP_SELECTED_COLOR = "#9400ff";
const POPUP_SELECTED_TEXT_COLOR = "#09b9a6";

const POPUP_URL_COLOR_DARK = "#0061e0";
const POPUP_ACTION_COLOR_DARK = "#5b5b66";
const POPUP_URL_COLOR_BRIGHT = "#00ddff";
const POPUP_ACTION_COLOR_BRIGHT = "#bfbfc9";

const SEARCH_TERM = "urlbar-reflows-" + Date.now();

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

add_setup(async function() {
  await PlacesUtils.history.clear();
  const NUM_VISITS = 10;
  let visits = [];

  for (let i = 0; i < NUM_VISITS; ++i) {
    visits.push({
      uri: `http://example.com/urlbar-reflows-${i}`,
      title: `Reflow test for URL bar entry #${i} - ${SEARCH_TERM}`,
    });
  }

  await PlacesTestUtils.addVisits(visits);

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_popup_url() {
  // Load a manifest with popup_text being dark (bright background). Test for
  // dark text properties.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_field_focus: POPUP_COLOR_BRIGHT,
          toolbar_field_text_focus: POPUP_TEXT_COLOR_DARK,
          popup_highlight: POPUP_SELECTED_COLOR,
          popup_highlight_text: POPUP_SELECTED_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let maxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    await BrowserTestUtils.removeTab(tab);
  });

  let visits = [];

  for (let i = 0; i < maxResults; i++) {
    visits.push({ uri: makeURI("http://example.com/autocomplete/?" + i) });
  }

  await PlacesTestUtils.addVisits(visits);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "example.com/autocomplete",
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, maxResults - 1);

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    maxResults,
    "Should get maxResults=" + maxResults + " results"
  );

  // Set the selected attribute to true to test the highlight popup properties
  UrlbarTestUtils.setSelectedRowIndex(window, 1);
  let actionResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  let urlResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  let resultCS = window.getComputedStyle(urlResult.element.row._content);

  Assert.equal(
    resultCS.backgroundColor,
    `rgb(${hexToRGB(POPUP_SELECTED_COLOR).join(", ")})`,
    `Popup highlight background color should be set to ${POPUP_SELECTED_COLOR}`
  );

  Assert.equal(
    resultCS.color,
    `rgb(${hexToRGB(POPUP_SELECTED_TEXT_COLOR).join(", ")})`,
    `Popup highlight color should be set to ${POPUP_SELECTED_TEXT_COLOR}`
  );

  // Now set the index to somewhere not on the first two, so that we can test both
  // url and action text colors.
  UrlbarTestUtils.setSelectedRowIndex(window, 2);

  Assert.equal(
    window.getComputedStyle(urlResult.element.url).color,
    `rgb(${hexToRGB(POPUP_URL_COLOR_DARK).join(", ")})`,
    `Urlbar popup url color should be set to ${POPUP_URL_COLOR_DARK}`
  );

  Assert.equal(
    window.getComputedStyle(actionResult.element.action).color,
    `rgb(${hexToRGB(POPUP_ACTION_COLOR_DARK).join(", ")})`,
    `Urlbar popup action color should be set to ${POPUP_ACTION_COLOR_DARK}`
  );

  await extension.unload();

  // Load a manifest with popup_text being bright (dark background). Test for
  // bright text properties.
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          toolbar_field_focus: POPUP_COLOR_DARK,
          toolbar_field_text_focus: POPUP_TEXT_COLOR_BRIGHT,
          popup_highlight: POPUP_SELECTED_COLOR,
          popup_highlight_text: POPUP_SELECTED_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  Assert.equal(
    window.getComputedStyle(urlResult.element.url).color,
    `rgb(${hexToRGB(POPUP_URL_COLOR_BRIGHT).join(", ")})`,
    `Urlbar popup url color should be set to ${POPUP_URL_COLOR_BRIGHT}`
  );

  Assert.equal(
    window.getComputedStyle(actionResult.element.action).color,
    `rgb(${hexToRGB(POPUP_ACTION_COLOR_BRIGHT).join(", ")})`,
    `Urlbar popup action color should be set to ${POPUP_ACTION_COLOR_BRIGHT}`
  );

  await extension.unload();
});
