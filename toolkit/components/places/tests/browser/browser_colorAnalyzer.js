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

// to make async tests easier, push them into here and call nextStep
// when a test finishes
let tests = [];
function generatorTest() {
  while (tests.length > 0) {
    tests.shift()();
    yield undefined;
  }
}

/**
 * Passes the given uri to findRepresentativeColor.
 * If expected is null, you expect it to fail.
 * If expected is a function, it will call that function.
 * If expected is a color, you expect that color to be returned.
 * Message is used in the calls to is().
 */
function frcTest(uri, expected, message, skipNextStep) {
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
      if (!skipNextStep) {
        nextStep();
      }
    }
  );
}

/**
 * Handy function for getting an image into findRepresentativeColor and testing it.
 * Makes a canvas with the given dimensions, calls paintCanvasFunc with the 2d
 * context of the canvas, sticks the generated canvas into findRepresentativeColor.
 * See frcTest.
 */
function canvasTest(width, height, paintCanvasFunc, expected, message, skipNextStep) {
  let canvas = hiddenWindowDoc.createElementNS(XHTML_NS, "canvas");
  canvas.width = width;
  canvas.height = height;
  paintCanvasFunc(canvas.getContext("2d"));
  let uri = canvas.toDataURL();
  frcTest(uri, expected, message, skipNextStep);
}

// simple test - draw a red box in the center, make sure we get red back
tests.push(function test_redSquare() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "red";
    ctx.fillRect(2, 2, 12, 12);
  }, 0xFF0000, "redSquare analysis returns red");
});

// draw a blue square in one corner, red in the other, such that blue overlaps
// red by one pixel, making it the dominant color
tests.push(function test_blueOverlappingRed() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 8, 8);
    ctx.fillStyle = "blue";
    ctx.fillRect(7, 7, 8, 8);
  }, 0x0000FF, "blueOverlappingRed analysis returns blue");
});

// draw a red gradient next to a solid blue rectangle to ensure that a large
// block of similar colors beats out a smaller block of one color
tests.push(function test_redGradientBlueSolid() {
  canvasTest(16, 16, function(ctx) {
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
tests.push(function test_transparent() {
  canvasTest(16, 16, function(ctx) {
    //do nothing!
  }, null, "transparent analysis fails");
});

tests.push(function test_invalidURI() {
  frcTest("data:blah,Imnotavaliddatauri", null, "invalid URI analysis fails");
});

tests.push(function test_malformedPNGURI() {
  frcTest("data:image/png;base64,iVBORblahblahblah", null,
          "malformed PNG URI analysis fails");
});

tests.push(function test_unresolvableURI() {
  frcTest("http://www.example.com/blah/idontexist.png", null,
          "unresolvable URI analysis fails");
});

// draw a small blue box on a red background to make sure the algorithm avoids
// using the background color
tests.push(function test_blueOnRedBackground() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "blue";
    ctx.fillRect(4, 4, 8, 8);
  }, 0x0000FF, "blueOnRedBackground analysis returns blue");
});

// draw a slightly different color in the corners to make sure the corner colors
// don't have to be exactly equal to be considered the background color
tests.push(function test_variableBackground() {
  canvasTest(16, 16, function(ctx) {
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
tests.push(function test_tooVariableBackground() {
  canvasTest(16, 16, function(ctx) {
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
tests.push(function test_transparentBackgroundConflation() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "black";
    ctx.fillRect(2, 2, 12, 12);
    ctx.fillStyle = "white";
    ctx.fillRect(5, 5, 6, 6);
  }, 0x000000, "transparentBackgroundConflation analysis returns black");
});

// make sure we fall back to the background color if we have no other choice
// (instead of failing as if there were no colors)
tests.push(function test_backgroundFallback() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "black";
    ctx.fillRect(0, 0, 16, 16);
  }, 0x000000, "backgroundFallback analysis returns black");
});

// draw red rectangle next to a pink one to make sure the algorithm picks the
// more interesting color
tests.push(function test_interestingColorPreference() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "#FFDDDD";
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 3, 16);
  }, 0xFF0000, "interestingColorPreference analysis returns red");
});

// draw high saturation but dark red next to slightly less saturated color but
// much lighter, to make sure the algorithm doesn't pick colors that are
// nearly black just because of high saturation (in HSL terms)
tests.push(function test_saturationDependence() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "hsl(0, 100%, 5%)";
    ctx.fillRect(0, 0, 16, 16);
    ctx.fillStyle = "hsl(0, 90%, 35%)";
    ctx.fillRect(0, 0, 8, 16);
  }, 0xA90808, "saturationDependence analysis returns lighter red");
});

// make sure the preference for interesting colors won't stupidly pick 1 pixel
// of red over 169 black pixels
tests.push(function test_interestingColorPreferenceLenient() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "black";
    ctx.fillRect(1, 1, 13, 13);
    ctx.fillStyle = "red";
    ctx.fillRect(3, 3, 1, 1);
  }, 0x000000, "interestingColorPreferenceLenient analysis returns black");
});

// ...but 6 pixels of red is more reasonable
tests.push(function test_interestingColorPreferenceNotTooLenient() {
  canvasTest(16, 16, function(ctx) {
    ctx.fillStyle = "black";
    ctx.fillRect(1, 1, 13, 13);
    ctx.fillStyle = "red";
    ctx.fillRect(3, 3, 3, 2);
  }, 0xFF0000, "interestingColorPreferenceNotTooLenient analysis returns red");
});

let maxPixels = 144; // see ColorAnalyzer MAXIMUM_PIXELS const

// make sure that images larger than maxPixels*maxPixels fail
tests.push(function test_imageTooLarge() {
  canvasTest(1+maxPixels, 1+maxPixels, function(ctx) {
    ctx.fillStyle = "red";
    ctx.fillRect(0, 0, 1+maxPixels, 1+maxPixels);
  }, null, "imageTooLarge analysis fails");
});


// these next tests are for performance (and also to make sure concurrency
// doesn't break anything)

let maxColor = Math.pow(2, 24) - 1;

function getRandomColor() {
  let randomColor = (Math.ceil(Math.random() * maxColor)).toString(16);
  return "000000".slice(0, 6 - randomColor.length) + randomColor;
}

function testFiller(color, ctx) {
  ctx.fillStyle = "#" + color;
  ctx.fillRect(2, 2, 12, 12);
}

tests.push(function test_perfInSeries() {
  let t1 = new Date();
  let numTests = 20;
  let allCorrect = true;
  function nextPerfTest() {
    let color = getRandomColor();
    canvasTest(16, 16, testFiller.bind(this, color), function(actual) {
      if (actual != parseInt(color, 16)) {
        allCorrect = false;
      }
      if (--numTests > 0) {
        nextPerfTest();
      } else {
        is(allCorrect, true, "perfInSeries colors are all correct");
        info("perfInSeries: " + ((new Date()) - t1) + "ms");
        nextStep();
      }
    }, "", true);
  }
  nextPerfTest();
});

tests.push(function test_perfInParallel() {
  let t1 = new Date();
  let numTests = 20;
  let testsDone = 0;
  let allCorrect = true;
  for (let i = 0; i < numTests; i++) {
    let color = getRandomColor();
    canvasTest(16, 16, testFiller.bind(this, color), function(actual) {
      if (actual != parseInt(color, 16)) {
        allCorrect = false;
      }
      if (++testsDone >= 20) {
        is(allCorrect, true, "perfInParallel colors are all correct");
        info("perfInParallel: " + ((new Date()) - t1) + "ms");
        nextStep();
      }
    }, "", true);
  }
});

tests.push(function test_perfBigImage() {
  let t1 = 0;
  canvasTest(128, 128, function(ctx) {
    // make sure to use a bunch of unique colors so the clustering algorithm
    // actually has to do work
    for (let y = 0; y < 128; y++) {
      for (let x = 0; x < 128; x++) {
        ctx.fillStyle = "#" + getRandomColor();
        ctx.fillRect(x, y, 1, 1);
      }
    }
    t1 = new Date();
  }, function(actual) {
    info("perfBigImage: " + ((new Date()) - t1) + "ms");
    nextStep();
  }, "", true);
});


// the rest of the tests are for coverage of "real" favicons
// exact color isn't terribly important, just make sure it's reasonable
const filePrefix = getRootDirectory(gTestPath) + "colorAnalyzer/";

tests.push(function test_categoryDiscover() {
  frcTest(filePrefix + "category-discover.png", 0xB28D3A,
          "category-discover analysis returns red");
});

tests.push(function test_localeGeneric() {
  frcTest(filePrefix + "localeGeneric.png", 0x3EC23E,
          "localeGeneric analysis returns green");
});

tests.push(function test_dictionaryGeneric() {
  frcTest(filePrefix + "dictionaryGeneric-16.png", 0x854C30,
          "dictionaryGeneric-16 analysis returns brown");
});

tests.push(function test_extensionGeneric() {
  frcTest(filePrefix + "extensionGeneric-16.png", 0x53BA3F,
          "extensionGeneric-16 analysis returns green");
});
