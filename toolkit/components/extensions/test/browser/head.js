/* exported ACCENT_COLOR, BACKGROUND, ENCODED_IMAGE_DATA, FRAME_COLOR, TAB_TEXT_COLOR,
   TEXT_COLOR, TAB_BACKGROUND_TEXT_COLOR, imageBufferFromDataURI, hexToCSS, hexToRGB, testBorderColor,
   waitForTransition, checkThemeHeaderImage */

"use strict";

const BACKGROUND = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0" +
  "DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
const ENCODED_IMAGE_DATA = "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";
const ACCENT_COLOR = "#a14040";
const TEXT_COLOR = "#fac96e";
// For testing aliases of the colors above:
const FRAME_COLOR = [71, 105, 91];
const TAB_BACKGROUND_TEXT_COLOR = [207, 221, 192, .9];

function hexToRGB(hex) {
  if (!hex) {
    return null;
  }
  hex = parseInt((hex.indexOf("#") > -1 ? hex.substring(1) : hex), 16);
  return [hex >> 16, (hex & 0x00FF00) >> 8, (hex & 0x0000FF)];
}

function rgbToCSS(rgb) {
  return `rgb(${rgb.join(", ")})`;
}

function hexToCSS(hex) {
  if (!hex) {
    return null;
  }
  return rgbToCSS(hexToRGB(hex));
}

function imageBufferFromDataURI(encodedImageData) {
  let decodedImageData = atob(encodedImageData);
  return Uint8Array.from(decodedImageData, byte => byte.charCodeAt(0)).buffer;
}

function waitForTransition(element, propertyName) {
  return BrowserTestUtils.waitForEvent(element, "transitionend", false, event => {
    return event.target == element && event.propertyName == propertyName;
  });
}

function testBorderColor(element, expected) {
  let computedStyle = window.getComputedStyle(element);
  Assert.equal(computedStyle.borderLeftColor,
               hexToCSS(expected),
               "Element left border color should be set.");
  Assert.equal(computedStyle.borderRightColor,
               hexToCSS(expected),
               "Element right border color should be set.");
  Assert.equal(computedStyle.borderTopColor,
               hexToCSS(expected),
               "Element top border color should be set.");
  Assert.equal(computedStyle.borderBottomColor,
               hexToCSS(expected),
               "Element bottom border color should be set.");
}

function checkThemeHeaderImage(window, expectedURL) {
  const {LightweightThemeManager} = ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm");

  let root = window.document.documentElement;
  if (expectedURL === "none") {
    Assert.equal(window.getComputedStyle(root).backgroundImage,
                 "none", "Should have no background image");
  } else {
    Assert.equal(window.getComputedStyle(root).backgroundImage,
                 "-moz-element(#lwt-header-image)",
                 "Should have -moz-element background image");

    Assert.equal(LightweightThemeManager.themeData.theme.headerImage.src,
                 expectedURL, "Theme image has expected source");
  }
}
