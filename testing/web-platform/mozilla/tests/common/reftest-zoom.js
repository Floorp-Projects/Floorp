// This JS file allows to emulate reftest-zoom.
// See https://firefox-source-docs.mozilla.org/layout/Reftest.html#zoom-tests-reftest-zoom-float

// Retrieve reftest-zoom attribute.
const reftestZoom = "reftest-zoom";
const root = document.documentElement;
if (!root.hasAttribute(reftestZoom)) {
  throw new Error(`${reftestZoom} attribute not found on the root element.`);
}

// Parse reftest-zoom value.
let zoom = parseFloat(root.getAttribute(reftestZoom));
if (Number.isNaN(zoom)) {
  throw new Error(`${reftestZoom} is not a float number.`);
}

// Clamp reftest-zoom value.
let minZoom = SpecialPowers.getIntPref("zoom.minPercent") / 100;
let maxZoom = SpecialPowers.getIntPref("zoom.maxPercent") / 100;
zoom = Math.min(Math.max(zoom, minZoom), maxZoom);

// Ensure the original zoom level is restored after the screenshot.
const originalZoom = SpecialPowers.getFullZoom(window);
window.addEventListener("beforeunload", () => {
  SpecialPowers.setFullZoom(window, originalZoom);
});

// Set the zoom level to the specified value.
SpecialPowers.setFullZoom(window, zoom);
