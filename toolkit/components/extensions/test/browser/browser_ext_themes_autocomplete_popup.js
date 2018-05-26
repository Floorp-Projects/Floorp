"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// popup properties are applied correctly to the autocomplete bar.
const POPUP_COLOR = "#85A400";
const POPUP_BORDER_COLOR = "#220300";
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
});

function promisePopupShown(popup) {
  return new Promise(resolve => {
    if (popup.state == "open") {
      resolve();
    } else {
      popup.addEventListener("popupshown", function(event) {
        resolve();
      }, {once: true});
    }
  });
}

async function promiseAutocompleteResultPopup(inputText) {
  gURLBar.focus();
  gURLBar.value = inputText;
  gURLBar.controller.startSearch(inputText);
  await promisePopupShown(gURLBar.popup);
  await BrowserTestUtils.waitForCondition(() => {
    return gURLBar.controller.searchStatus >=
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
  });
}

async function waitForAutocompleteResultAt(index) {
  let searchString = gURLBar.controller.searchString;
  await BrowserTestUtils.waitForCondition(
    () => gURLBar.popup.richlistbox.children.length > index &&
          gURLBar.popup.richlistbox.children[index].getAttribute("ac-text") == searchString,
    `Waiting for the autocomplete result for "${searchString}" at [${index}] to appear`);
  // Ensure the addition is complete, for proper mouse events on the entries.
  await new Promise(resolve => window.requestIdleCallback(resolve, {timeout: 1000}));
  return gURLBar.popup.richlistbox.children[index];
}

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
  // Load extension with brighttext not set
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
          "popup": POPUP_COLOR,
          "popup_border": POPUP_BORDER_COLOR,
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
  await promiseAutocompleteResultPopup("example.com/autocomplete");
  await waitForAutocompleteResultAt(maxResults - 1);

  let popup = gURLBar.popup;
  let results = popup.richlistbox.children;
  is(results.length, maxResults,
     "Should get maxResults=" + maxResults + " results");

  let popupCS = window.getComputedStyle(popup);

  Assert.equal(popupCS.backgroundColor,
               `rgb(${hexToRGB(POPUP_COLOR).join(", ")})`,
               `Popup background color should be set to ${POPUP_COLOR}`);

  testBorderColor(popup, POPUP_BORDER_COLOR);

  Assert.equal(popupCS.color,
               `rgb(${hexToRGB(POPUP_TEXT_COLOR_DARK).join(", ")})`,
               `Popup color should be set to ${POPUP_TEXT_COLOR_DARK}`);

  // Set the selected attribute to true to test the highlight popup properties
  results[1].setAttribute("selected", "true");
  let resultCS = window.getComputedStyle(results[1]);

  Assert.equal(resultCS.backgroundColor,
               `rgb(${hexToRGB(POPUP_SELECTED_COLOR).join(", ")})`,
               `Popup highlight background color should be set to ${POPUP_SELECTED_COLOR}`);

  Assert.equal(resultCS.color,
               `rgb(${hexToRGB(POPUP_SELECTED_TEXT_COLOR).join(", ")})`,
               `Popup highlight color should be set to ${POPUP_SELECTED_TEXT_COLOR}`);

  results[1].removeAttribute("selected");

  let urlText = document.getAnonymousElementByAttribute(results[1], "anonid", "url-text");
  Assert.equal(window.getComputedStyle(urlText).color,
               `rgb(${hexToRGB(POPUP_URL_COLOR_DARK).join(", ")})`,
               `Urlbar popup url color should be set to ${POPUP_URL_COLOR_DARK}`);

  let actionText = document.getAnonymousElementByAttribute(results[1], "anonid", "action-text");
  Assert.equal(window.getComputedStyle(actionText).color,
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
          "headerURL": "image1.png",
        },
        "colors": {
          "accentcolor": ACCENT_COLOR,
          "textcolor": TEXT_COLOR,
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

  urlText = document.getAnonymousElementByAttribute(results[1], "anonid", "url-text");
  Assert.equal(window.getComputedStyle(urlText).color,
               `rgb(${hexToRGB(POPUP_URL_COLOR_BRIGHT).join(", ")})`,
               `Urlbar popup url color should be set to ${POPUP_URL_COLOR_BRIGHT}`);

  actionText = document.getAnonymousElementByAttribute(results[1], "anonid", "action-text");
  Assert.equal(window.getComputedStyle(actionText).color,
               `rgb(${hexToRGB(POPUP_ACTION_COLOR_BRIGHT).join(", ")})`,
               `Urlbar popup action color should be set to ${POPUP_ACTION_COLOR_BRIGHT}`);

  // Since brighttext is enabled, the seperator color should be
  // POPUP_TEXT_COLOR_BRIGHT with added alpha.
  let separator = document.getAnonymousElementByAttribute(results[1], "anonid", "separator");
  Assert.equal(window.getComputedStyle(separator).color,
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
  let span = document.createElement("span");
  span.style.color = "GrayText";
  let GRAY_TEXT = window.getComputedStyle(span).color;

  separator = document.getAnonymousElementByAttribute(results[1], "anonid", "separator");
  Assert.equal(window.getComputedStyle(separator).color,
               GRAY_TEXT,
               `Urlbar popup separator color should be set to ${GRAY_TEXT}`);
});
