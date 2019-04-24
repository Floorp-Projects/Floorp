"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// popup properties are applied correctly to the autocomplete bar.
const POPUP_COLOR = "#85A400";
const POPUP_BORDER_COLOR = "#220300";
const CHROME_CONTENT_SEPARATOR_COLOR = "#220301";
const POPUP_TEXT_COLOR_DARK = "#000000";
const POPUP_TEXT_COLOR_BRIGHT = "#ffffff";
const POPUP_SELECTED_COLOR = "#9400ff";
const POPUP_SELECTED_TEXT_COLOR = "#09b9a6";

const POPUP_URL_COLOR_DARK = "#1c78d4";
const POPUP_ACTION_COLOR_DARK = "#008f8a";
const POPUP_URL_COLOR_BRIGHT = "#45a1ff";
const POPUP_ACTION_COLOR_BRIGHT = "#30e60b";

const SEARCH_TERM = "urlbar-reflows-" + Date.now();
const ONEOFF_URLBAR_PREF = "browser.urlbar.oneOffSearches";

XPCOMUtils.defineLazyModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

add_task(async function setup() {
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
  const quantumbar = UrlbarPrefs.get("quantumbar");

  // Load extension with brighttext not set
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "image1.png",
        },
        "colors": {
          "frame": ACCENT_COLOR,
          "tab_background_text": TEXT_COLOR,
          "popup": POPUP_COLOR,
          "popup_border": POPUP_BORDER_COLOR,
          "toolbar_bottom_separator": CHROME_CONTENT_SEPARATOR_COLOR,
          "popup_text": POPUP_TEXT_COLOR_DARK,
          "popup_highlight": POPUP_SELECTED_COLOR,
          "popup_highlight_text": POPUP_SELECTED_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  let maxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");
  Services.prefs.setBoolPref(ONEOFF_URLBAR_PREF, true);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    Services.prefs.clearUserPref(ONEOFF_URLBAR_PREF);
    await BrowserTestUtils.removeTab(tab);
  });

  let visits = [];

  for (let i = 0; i < maxResults; i++) {
    visits.push({uri: makeURI("http://example.com/autocomplete/?" + i)});
  }

  await PlacesTestUtils.addVisits(visits);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "example.com/autocomplete",
  });
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, maxResults - 1);

  Assert.equal(UrlbarTestUtils.getResultCount(window), maxResults,
               "Should get maxResults=" + maxResults + " results");

  let popup = UrlbarTestUtils.getPanel(window);
  let popupCS = window.getComputedStyle(popup);

  Assert.equal(popupCS.backgroundColor,
               `rgb(${hexToRGB(POPUP_COLOR).join(", ")})`,
               `Popup background color should be set to ${POPUP_COLOR}`);

  if (quantumbar) {
    Assert.equal(popupCS.borderBottomColor,
                 `rgb(${hexToRGB(CHROME_CONTENT_SEPARATOR_COLOR).join(", ")})`,
                 `Popup bottom color should be set to ${CHROME_CONTENT_SEPARATOR_COLOR}`);
  } else {
    testBorderColor(popup, POPUP_BORDER_COLOR);
  }

  Assert.equal(popupCS.color,
               `rgb(${hexToRGB(POPUP_TEXT_COLOR_DARK).join(", ")})`,
               `Popup color should be set to ${POPUP_TEXT_COLOR_DARK}`);

  // Set the selected attribute to true to test the highlight popup properties
  UrlbarTestUtils.setSelectedIndex(window, 1);
  let actionResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  let urlResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  let resultCS = window.getComputedStyle(urlResult.element.row);

  Assert.equal(resultCS.backgroundColor,
               `rgb(${hexToRGB(POPUP_SELECTED_COLOR).join(", ")})`,
               `Popup highlight background color should be set to ${POPUP_SELECTED_COLOR}`);

  Assert.equal(resultCS.color,
               `rgb(${hexToRGB(POPUP_SELECTED_TEXT_COLOR).join(", ")})`,
               `Popup highlight color should be set to ${POPUP_SELECTED_TEXT_COLOR}`);

  // Now set the index to somewhere not on the first two, so that we can test both
  // url and action text colors.
  UrlbarTestUtils.setSelectedIndex(window, 2);

  Assert.equal(window.getComputedStyle(urlResult.element.url).color,
               `rgb(${hexToRGB(POPUP_URL_COLOR_DARK).join(", ")})`,
               `Urlbar popup url color should be set to ${POPUP_URL_COLOR_DARK}`);

  Assert.equal(window.getComputedStyle(actionResult.element.action).color,
               `rgb(${hexToRGB(POPUP_ACTION_COLOR_DARK).join(", ")})`,
               `Urlbar popup action color should be set to ${POPUP_ACTION_COLOR_DARK}`);

  let root = document.documentElement;
  Assert.equal(root.getAttribute("lwt-popup-brighttext"),
               "",
               "brighttext should not be set!");
  Assert.equal(root.getAttribute("lwt-popup-darktext"),
               "true",
               "darktext should be set!");

  await extension.unload();

  Assert.equal(root.hasAttribute("lwt-popup-darktext"),
               false,
               "lwt-popup-darktext attribute should be removed");

  // Load a manifest with popup_text being bright. Test for bright text properties.
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "theme_frame": "image1.png",
        },
        "colors": {
          "frame": ACCENT_COLOR,
          "tab_background_text": TEXT_COLOR,
          "popup": POPUP_COLOR,
          "popup_text": POPUP_TEXT_COLOR_BRIGHT,
          "popup_highlight": POPUP_SELECTED_COLOR,
          "popup_highlight_text": POPUP_SELECTED_TEXT_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await extension.startup();

  popupCS = window.getComputedStyle(popup);
  Assert.equal(popupCS.color,
               `rgb(${hexToRGB(POPUP_TEXT_COLOR_BRIGHT).join(", ")})`,
               `Popup color should be set to ${POPUP_TEXT_COLOR_BRIGHT}`);

  Assert.equal(window.getComputedStyle(urlResult.element.url).color,
               `rgb(${hexToRGB(POPUP_URL_COLOR_BRIGHT).join(", ")})`,
               `Urlbar popup url color should be set to ${POPUP_URL_COLOR_BRIGHT}`);

  Assert.equal(window.getComputedStyle(actionResult.element.action).color,
               `rgb(${hexToRGB(POPUP_ACTION_COLOR_BRIGHT).join(", ")})`,
               `Urlbar popup action color should be set to ${POPUP_ACTION_COLOR_BRIGHT}`);

  // Since brighttext is enabled, the seperator color should be
  // POPUP_TEXT_COLOR_BRIGHT with added alpha.
  Assert.equal(window.getComputedStyle(urlResult.element.separator, quantumbar ? ":before" : null).color,
               `rgba(${hexToRGB(POPUP_TEXT_COLOR_BRIGHT).join(", ")}, 0.5)`,
               `Urlbar popup separator color should be set to ${POPUP_TEXT_COLOR_BRIGHT} with alpha`);

  Assert.equal(root.getAttribute("lwt-popup-brighttext"),
               "true",
               "brighttext should be set to true!");
  Assert.equal(root.getAttribute("lwt-popup-darktext"),
               "",
               "darktext should not be set!");

  await extension.unload();

  // Check to see if popup-brighttext and secondary color are not set after
  // unload of theme
  Assert.equal(root.getAttribute("lwt-popup-brighttext"),
               "",
               "brighttext should not be set!");
  Assert.equal(root.getAttribute("lwt-popup-darktext"),
               "",
               "darktext should not be set!");

  // Calculate what GrayText should be. May differ between platforms.
  let span = document.createXULElement("span");
  span.style.color = "GrayText";
  document.documentElement.appendChild(span);
  let GRAY_TEXT = window.getComputedStyle(span).color;
  span.remove();

  Assert.equal(window.getComputedStyle(urlResult.element.separator, quantumbar ? ":before" : null).color,
               GRAY_TEXT,
               `Urlbar popup separator color should be set to ${GRAY_TEXT}`);
});
