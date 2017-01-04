/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/browser/toolkit/components/thumbnails/" +
            "test/background_red_scroll.html";

function isRedThumbnailFuzz(r, g, b, expectedR, expectedB, expectedG, aFuzz) {
  return (Math.abs(r - expectedR) <= aFuzz) &&
         (Math.abs(g - expectedG) <= aFuzz) &&
         (Math.abs(b - expectedB) <= aFuzz);
}

// Test for black borders caused by scrollbars.
function* runTests() {
  // Create a tab with a page with a red background and scrollbars.
  yield addTab(URL);
  yield captureAndCheckColor(255, 0, 0, "we have a red thumbnail");

  // Check the thumbnail color of the bottom right pixel.
  yield whenFileExists(URL);
  yield retrieveImageDataForURL(URL, function(aData) {
    let [r, g, b] = [].slice.call(aData, -4);
    let fuzz = 2; // Windows 8 x64 blends with the scrollbar a bit.
    var message = "Expected red thumbnail rgb(255, 0, 0), got " + r + "," + g + "," + b;
    ok(isRedThumbnailFuzz(r, g, b, 255, 0, 0, fuzz), message);
    next();
  });
}
