/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource://gre/modules/Services.jsm");

const CA = Cc["@mozilla.org/places/colorAnalyzer;1"].
           getService(Ci.mozIColorAnalyzer);

const hiddenWindowDoc = Cc["@mozilla.org/appshell/appShellService;1"].
                        getService(Ci.nsIAppShellService).
                        hiddenDOMWindow.document;

const XHTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * Passes the given uri to findRepresentativeColor.
 * If expected is null, you expect it to fail.
 * If expected is a function, it will call that function.
 * If expected is a color, you expect that color to be returned.
 * Message is used in the calls to is().
 */
function frcTest(uri, expected, message) {
  return new Promise(resolve => {
    CA.findRepresentativeColor(Services.io.newURI(uri, "", null),
      function(success, color) {
        if (expected == null) {
          ok(!success, message);
        } else if (typeof expected == "function") {
          expected(color, message);
        } else {
          ok(success, "success: " + message);
          is(color, expected, message);
        }
        resolve();
    });
  });
}

/**
 * Handy function for getting an image into findRepresentativeColor and testing it.
 * Makes a canvas with the given dimensions, calls paintCanvasFunc with the 2d
 * context of the canvas, sticks the generated canvas into findRepresentativeColor.
 * See frcTest.
 */
function canvasTest(width, height, paintCanvasFunc, expected, message) {
  let canvas = hiddenWindowDoc.createElementNS(XHTML_NS, "canvas");
  canvas.width = width;
  canvas.height = height;
  paintCanvasFunc(canvas.getContext("2d"));
  let uri = canvas.toDataURL();
  return frcTest(uri, expected, message);
}

// simple test - draw a red box in the center, make sure we get red back
add_task(function* test_redSquare() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "red";
    ctx.fillRect(2, 2, 12, 12);
  }, 0xFF0000, "redSquare analysis returns red");
});


// draw a blue square in one corner, red in the other, such that blue overlaps
// red by one pixel, making it the dominant color
add_task(function* test_blueOverlappingRed() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 8, 8);
    ctx.fillStyle = "blue";
    ctx.fillRect(7, 7, 8, 8);
  }, 0x0000FF, "blueOverlappingRed analysis returns blue");
});

// draw a red gradient next to a solid blue rectangle to ensure that a large
// block of similar colors beats out a smaller block of one color
add_task(function* test_redGradientBlueSolid() {
  yield canvasTest(16, 16, function(ctx) {
    let gradient = ctx.createLinearGradient(0, 0, 1, 15);
    gradient.addColorStop(0, "#FF0000");
    gradient.addColorStop(1, "#FF0808");

    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "blue";
    ctx.fillRect(9, 0, 7, 16);
  }, function(actual, message) {
    ok(actual >= 0xFF0000 && actual <= 0xFF0808, message);
  }, "redGradientBlueSolid analysis returns redish");
});

// try a transparent image, should fail
add_task(function* test_transparent() {
  yield canvasTest(16, 16, function(ctx) {
    //do nothing!
  }, null, "transparent analysis fails");
});

add_task(function* test_invalidURI() {
  yield frcTest("data:blah,Imnotavaliddatauri", null, "invalid URI analysis fails");
});

add_task(function* test_malformedPNGURI() {
  yield frcTest("data:image/png;base64,iVBORblahblahblah", null,
                "malformed PNG URI analysis fails");
});

add_task(function* test_unresolvableURI() {
  yield frcTest("http://www.example.com/blah/idontexist.png", null,
                "unresolvable URI analysis fails");
});

// draw a small blue box on a red background to make sure the algorithm avoids
// using the background color
add_task(function* test_blueOnRedBackground() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "blue";
    ctx.fillRect(4, 4, 8, 8);
  }, 0x0000FF, "blueOnRedBackground analysis returns blue");
});

// draw a slightly different color in the corners to make sure the corner colors
// don't have to be exactly equal to be considered the background color
add_task(function* test_variableBackground() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "white";
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "#FEFEFE";
    ctx.fillRect(15, 0, 1, 1);
    ctx.fillStyle = "#FDFDFD";
    ctx.fillRect(15, 15, 1, 1);
    ctx.fillStyle = "#FCFCFC";
    ctx.fillRect(0, 15, 1, 1);
    ctx.fillStyle = "black";
    ctx.fillRect(4, 4, 8, 8);
  }, 0x000000, "variableBackground analysis returns black");
});

// like the above test, but make the colors different enough that they aren't
// considered the background color
add_task(function* test_tooVariableBackground() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "white";
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "#EEDDCC";
    ctx.fillRect(15, 0, 1, 1);
    ctx.fillStyle = "#DDDDDD";
    ctx.fillRect(15, 15, 1, 1);
    ctx.fillStyle = "#CCCCCC";
    ctx.fillRect(0, 15, 1, 1);
    ctx.fillStyle = "black";
    ctx.fillRect(4, 4, 8, 8);
  }, function(actual, message) {
    isnot(actual, 0x000000, message);
  }, "tooVariableBackground analysis doesn't return black");
});

// draw a small black/white box over transparent background to make sure the
// algorithm doesn't think rgb(0,0,0) == rgba(0,0,0,0)
add_task(function* test_transparentBackgroundConflation() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "black";
    ctx.fillRect(2, 2, 12, 12);
    ctx.fillStyle = "white";
    ctx.fillRect(5, 5, 6, 6);
  }, 0x000000, "transparentBackgroundConflation analysis returns black");
});


// make sure we fall back to the background color if we have no other choice
// (instead of failing as if there were no colors)
add_task(function* test_backgroundFallback() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "black";
    ctx.fillRect(0, 0, 16, 16);
  }, 0x000000, "backgroundFallback analysis returns black");
});

// draw red rectangle next to a pink one to make sure the algorithm picks the
// more interesting color
add_task(function* test_interestingColorPreference() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "#FFDDDD";
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 3, 16);
  }, 0xFF0000, "interestingColorPreference analysis returns red");
});

// draw high saturation but dark red next to slightly less saturated color but
// much lighter, to make sure the algorithm doesn't pick colors that are
// nearly black just because of high saturation (in HSL terms)
add_task(function* test_saturationDependence() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "hsl(0, 100%, 5%)";
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "hsl(0, 90%, 35%)";
    ctx.fillRect(0, 0, 8, 16);
  }, 0xA90808, "saturationDependence analysis returns lighter red");
});

// make sure the preference for interesting colors won't stupidly pick 1 pixel
// of red over 169 black pixels
add_task(function* test_interestingColorPreferenceLenient() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "black";
    ctx.fillRect(1, 1, 13, 13);
    ctx.fillStyle = "red";
    ctx.fillRect(3, 3, 1, 1);
  }, 0x000000, "interestingColorPreferenceLenient analysis returns black");
});

// ...but 6 pixels of red is more reasonable
add_task(function* test_interestingColorPreferenceNotTooLenient() {
  yield canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "black";
    ctx.fillRect(1, 1, 13, 13);
    ctx.fillStyle = "red";
    ctx.fillRect(3, 3, 3, 2);
  }, 0xFF0000, "interestingColorPreferenceNotTooLenient analysis returns red");
});

var maxPixels = 144; // see ColorAnalyzer MAXIMUM_PIXELS const

// make sure that images larger than maxPixels*maxPixels fail
add_task(function* test_imageTooLarge() {
  yield canvasTest(1+maxPixels, 1+maxPixels, function(ctx) {
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 1+maxPixels, 1+maxPixels);
  }, null, "imageTooLarge analysis fails");
});

// the rest of the tests are for coverage of "real" favicons
// exact color isn't terribly important, just make sure it's reasonable
const filePrefix = getRootDirectory(gTestPath) + "colorAnalyzer/";

add_task(function* test_categoryDiscover() {
  yield frcTest(filePrefix + "category-discover.png", 0xB28D3A,
                "category-discover analysis returns red");
});

add_task(function* test_localeGeneric() {
  yield frcTest(filePrefix + "localeGeneric.png", 0x3EC23E,
                "localeGeneric analysis returns green");
});

add_task(function* test_dictionaryGeneric() {
  yield frcTest(filePrefix + "dictionaryGeneric-16.png", 0x854C30,
                "dictionaryGeneric-16 analysis returns brown");
});

add_task(function* test_extensionGeneric() {
  yield frcTest(filePrefix + "extensionGeneric-16.png", 0x53BA3F,
                "extensionGeneric-16 analysis returns green");
});
