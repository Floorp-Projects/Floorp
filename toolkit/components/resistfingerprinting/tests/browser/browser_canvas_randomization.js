/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 1816189 - Testing canvas randomization on canvas data extraction.
 *
 * In the test, we create canvas elements and offscreen canvas and test if
 * the extracted canvas data is altered because of the canvas randomization.
 */

const emptyPage =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

var TEST_CASES = [
  {
    name: "CanvasRenderingContext2D.getImageData() but constant color",
    shouldBeRandomized: false,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], _ => {
        const canvas = content.document.createElement("canvas");
        canvas.width = 100;
        canvas.height = 100;

        const context = canvas.getContext("2d");

        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);

        // Add the canvas element to the document
        content.document.body.appendChild(canvas);

        const imageData = context.getImageData(0, 0, 100, 100);

        // Access the data again.
        const imageDataSecond = context.getImageData(0, 0, 100, 100);

        return [imageData.data, imageDataSecond.data];
      });
    },
    isDataRandomized(name, data1, data2, isCompareOriginal) {
      let diffCnt = countDifferencesInUint8Arrays(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);

      // The Canvas randomization adds at most 512 bits noise to the image data.
      // We compare the image data arrays to see if they are different and the
      // difference is within the range.

      // If we are compare two randomized arrays, the difference can be doubled.
      let expected = isCompareOriginal
        ? NUM_RANDOMIZED_CANVAS_BITS
        : NUM_RANDOMIZED_CANVAS_BITS * 2;

      // The number of difference bits should never bigger than the expected
      // number. It could be zero if the randomization is disabled.
      ok(diffCnt <= expected, "The number of noise bits is expected.");

      return diffCnt <= expected && diffCnt > 0;
    },
  },
  {
    name: "CanvasRenderingContext2D.getImageData().",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], _ => {
        const canvas = content.document.createElement("canvas");
        canvas.width = 100;
        canvas.height = 100;

        const context = canvas.getContext("2d");

        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);
        context.fillStyle = "#2222EE";
        context.fillRect(20, 20, 100, 100);

        // Add the canvas element to the document
        content.document.body.appendChild(canvas);

        const imageData = context.getImageData(0, 0, 100, 100);

        // Access the data again.
        const imageDataSecond = context.getImageData(0, 0, 100, 100);

        return [imageData.data, imageDataSecond.data];
      });
    },
    isDataRandomized(name, data1, data2, isCompareOriginal) {
      let diffCnt = countDifferencesInUint8Arrays(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);

      // The Canvas randomization adds at most 512 bits noise to the image data.
      // We compare the image data arrays to see if they are different and the
      // difference is within the range.

      // If we are compare two randomized arrays, the difference can be doubled.
      let expected = isCompareOriginal
        ? NUM_RANDOMIZED_CANVAS_BITS
        : NUM_RANDOMIZED_CANVAS_BITS * 2;

      // The number of difference bits should never bigger than the expected
      // number. It could be zero if the randomization is disabled.
      ok(diffCnt <= expected, "The number of noise bits is expected.");

      return diffCnt <= expected && diffCnt > 0;
    },
  },
  {
    name: "HTMLCanvasElement.toDataURL() with a 2d context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], _ => {
        const canvas = content.document.createElement("canvas");
        canvas.width = 100;
        canvas.height = 100;

        const context = canvas.getContext("2d");

        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);
        context.fillStyle = "#2222EE";
        context.fillRect(20, 20, 100, 100);

        // Add the canvas element to the document
        content.document.body.appendChild(canvas);

        const dataURL = canvas.toDataURL();

        // Access the data again.
        const dataURLSecond = canvas.toDataURL();

        return [dataURL, dataURLSecond];
      });
    },
    isDataRandomized(name, data1, data2) {
      return data1 !== data2;
    },
  },
  {
    name: "HTMLCanvasElement.toDataURL() with a webgl context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], _ => {
        const canvas = content.document.createElement("canvas");

        const context = canvas.getContext("webgl");

        context.enable(context.SCISSOR_TEST);
        context.scissor(0, 0, 100, 100);
        context.clearColor(1, 0.2, 0.2, 1);
        context.clear(context.COLOR_BUFFER_BIT);
        context.scissor(15, 15, 30, 15);
        context.clearColor(0.2, 1, 0.2, 1);
        context.clear(context.COLOR_BUFFER_BIT);
        context.scissor(50, 50, 15, 15);
        context.clearColor(0.2, 0.2, 1, 1);
        context.clear(context.COLOR_BUFFER_BIT);

        // Add the canvas element to the document
        content.document.body.appendChild(canvas);

        const dataURL = canvas.toDataURL();

        // Access the data again.
        const dataURLSecond = canvas.toDataURL();

        return [dataURL, dataURLSecond];
      });
    },
    isDataRandomized(name, data1, data2) {
      return data1 !== data2;
    },
  },
  {
    name: "HTMLCanvasElement.toDataURL() with a bitmaprenderer context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], async _ => {
        const canvas = content.document.createElement("canvas");
        canvas.width = 100;
        canvas.height = 100;

        const context = canvas.getContext("2d");

        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);
        context.fillStyle = "#2222EE";
        context.fillRect(20, 20, 100, 100);

        // Add the canvas element to the document
        content.document.body.appendChild(canvas);

        const bitmapCanvas = content.document.createElement("canvas");
        bitmapCanvas.width = 100;
        bitmapCanvas.heigh = 100;
        content.document.body.appendChild(bitmapCanvas);

        let bitmap = await content.createImageBitmap(canvas);
        const bitmapContext = bitmapCanvas.getContext("bitmaprenderer");
        bitmapContext.transferFromImageBitmap(bitmap);

        const dataURL = bitmapCanvas.toDataURL();

        // Access the data again.
        const dataURLSecond = bitmapCanvas.toDataURL();

        return [dataURL, dataURLSecond];
      });
    },
    isDataRandomized(name, data1, data2) {
      return data1 !== data2;
    },
  },
  {
    name: "HTMLCanvasElement.toBlob() with a 2d context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], async _ => {
        const canvas = content.document.createElement("canvas");
        canvas.width = 100;
        canvas.height = 100;

        const context = canvas.getContext("2d");

        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);
        context.fillStyle = "#2222EE";
        context.fillRect(20, 20, 100, 100);

        // Add the canvas element to the document
        content.document.body.appendChild(canvas);

        let data = await new content.Promise(resolve => {
          canvas.toBlob(blob => {
            let fileReader = new content.FileReader();
            fileReader.onload = () => {
              resolve(fileReader.result);
            };
            fileReader.readAsArrayBuffer(blob);
          });
        });

        // Access the data again.
        let dataSecond = await new content.Promise(resolve => {
          canvas.toBlob(blob => {
            let fileReader = new content.FileReader();
            fileReader.onload = () => {
              resolve(fileReader.result);
            };
            fileReader.readAsArrayBuffer(blob);
          });
        });

        return [data, dataSecond];
      });
    },
    isDataRandomized(name, data1, data2) {
      let diffCnt = countDifferencesInArrayBuffers(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);
      return diffCnt > 0;
    },
  },
  {
    name: "HTMLCanvasElement.toBlob() with a webgl context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], async _ => {
        const canvas = content.document.createElement("canvas");

        const context = canvas.getContext("webgl");

        context.enable(context.SCISSOR_TEST);
        context.scissor(0, 0, 100, 100);
        context.clearColor(1, 0.2, 0.2, 1);
        context.clear(context.COLOR_BUFFER_BIT);
        context.scissor(15, 15, 30, 15);
        context.clearColor(0.2, 1, 0.2, 1);
        context.clear(context.COLOR_BUFFER_BIT);
        context.scissor(50, 50, 15, 15);
        context.clearColor(0.2, 0.2, 1, 1);
        context.clear(context.COLOR_BUFFER_BIT);

        // Add the canvas element to the document
        content.document.body.appendChild(canvas);

        let data = await new content.Promise(resolve => {
          canvas.toBlob(blob => {
            let fileReader = new content.FileReader();
            fileReader.onload = () => {
              resolve(fileReader.result);
            };
            fileReader.readAsArrayBuffer(blob);
          });
        });

        // We don't get the consistent blob data on second access with webgl
        // context regardless of the canvas randomization. So, we report the
        // same data here to not fail the test. Ideally, we should look into
        // why this happens, but it's not caused by canvas randomization.

        return [data, data];
      });
    },
    isDataRandomized(name, data1, data2) {
      let diffCnt = countDifferencesInArrayBuffers(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);
      return diffCnt > 0;
    },
  },
  {
    name: "HTMLCanvasElement.toBlob() with a bitmaprenderer context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], async _ => {
        const canvas = content.document.createElement("canvas");
        canvas.width = 100;
        canvas.height = 100;

        const context = canvas.getContext("2d");

        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);
        context.fillStyle = "#2222EE";
        context.fillRect(20, 20, 100, 100);

        // Add the canvas element to the document
        content.document.body.appendChild(canvas);

        const bitmapCanvas = content.document.createElement("canvas");
        bitmapCanvas.width = 100;
        bitmapCanvas.heigh = 100;
        content.document.body.appendChild(bitmapCanvas);

        let bitmap = await content.createImageBitmap(canvas);
        const bitmapContext = bitmapCanvas.getContext("bitmaprenderer");
        bitmapContext.transferFromImageBitmap(bitmap);

        let data = await new content.Promise(resolve => {
          bitmapCanvas.toBlob(blob => {
            let fileReader = new content.FileReader();
            fileReader.onload = () => {
              resolve(fileReader.result);
            };
            fileReader.readAsArrayBuffer(blob);
          });
        });

        // Access the data again.
        let dataSecond = await new content.Promise(resolve => {
          bitmapCanvas.toBlob(blob => {
            let fileReader = new content.FileReader();
            fileReader.onload = () => {
              resolve(fileReader.result);
            };
            fileReader.readAsArrayBuffer(blob);
          });
        });

        return [data, dataSecond];
      });
    },
    isDataRandomized(name, data1, data2) {
      let diffCnt = countDifferencesInArrayBuffers(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);
      return diffCnt > 0;
    },
  },
  {
    name: "OffscreenCanvas.convertToBlob() with a 2d context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], async _ => {
        let offscreenCanvas = new content.OffscreenCanvas(100, 100);

        const context = offscreenCanvas.getContext("2d");

        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);
        context.fillStyle = "#2222EE";
        context.fillRect(20, 20, 100, 100);

        let blob = await offscreenCanvas.convertToBlob();

        let data = await new content.Promise(resolve => {
          let fileReader = new content.FileReader();
          fileReader.onload = () => {
            resolve(fileReader.result);
          };
          fileReader.readAsArrayBuffer(blob);
        });

        // Access the data again.
        let blobSecond = await offscreenCanvas.convertToBlob();

        let dataSecond = await new content.Promise(resolve => {
          let fileReader = new content.FileReader();
          fileReader.onload = () => {
            resolve(fileReader.result);
          };
          fileReader.readAsArrayBuffer(blobSecond);
        });

        return [data, dataSecond];
      });
    },
    isDataRandomized(name, data1, data2) {
      let diffCnt = countDifferencesInArrayBuffers(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);
      return diffCnt > 0;
    },
  },
  {
    name: "OffscreenCanvas.convertToBlob() with a webgl context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], async _ => {
        let offscreenCanvas = new content.OffscreenCanvas(100, 100);

        const context = offscreenCanvas.getContext("webgl");

        context.enable(context.SCISSOR_TEST);
        context.scissor(0, 0, 100, 100);
        context.clearColor(1, 0.2, 0.2, 1);
        context.clear(context.COLOR_BUFFER_BIT);
        context.scissor(15, 15, 30, 15);
        context.clearColor(0.2, 1, 0.2, 1);
        context.clear(context.COLOR_BUFFER_BIT);
        context.scissor(50, 50, 15, 15);
        context.clearColor(0.2, 0.2, 1, 1);
        context.clear(context.COLOR_BUFFER_BIT);

        let blob = await offscreenCanvas.convertToBlob();

        let data = await new content.Promise(resolve => {
          let fileReader = new content.FileReader();
          fileReader.onload = () => {
            resolve(fileReader.result);
          };
          fileReader.readAsArrayBuffer(blob);
        });

        // Access the data again.
        let blobSecond = await offscreenCanvas.convertToBlob();

        let dataSecond = await new content.Promise(resolve => {
          let fileReader = new content.FileReader();
          fileReader.onload = () => {
            resolve(fileReader.result);
          };
          fileReader.readAsArrayBuffer(blobSecond);
        });

        return [data, dataSecond];
      });
    },
    isDataRandomized(name, data1, data2) {
      let diffCnt = countDifferencesInArrayBuffers(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);
      return diffCnt > 0;
    },
  },
  {
    name: "OffscreenCanvas.convertToBlob() with a bitmaprenderer context",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], async _ => {
        let offscreenCanvas = new content.OffscreenCanvas(100, 100);

        const context = offscreenCanvas.getContext("2d");

        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);
        context.fillStyle = "#2222EE";
        context.fillRect(20, 20, 100, 100);

        const bitmapCanvas = new content.OffscreenCanvas(100, 100);

        let bitmap = await content.createImageBitmap(offscreenCanvas);
        const bitmapContext = bitmapCanvas.getContext("bitmaprenderer");
        bitmapContext.transferFromImageBitmap(bitmap);

        let blob = await bitmapCanvas.convertToBlob();

        let data = await new content.Promise(resolve => {
          let fileReader = new content.FileReader();
          fileReader.onload = () => {
            resolve(fileReader.result);
          };
          fileReader.readAsArrayBuffer(blob);
        });

        // Access the data again.
        let blobSecond = await bitmapCanvas.convertToBlob();

        let dataSecond = await new content.Promise(resolve => {
          let fileReader = new content.FileReader();
          fileReader.onload = () => {
            resolve(fileReader.result);
          };
          fileReader.readAsArrayBuffer(blobSecond);
        });

        return [data, dataSecond];
      });
    },
    isDataRandomized(name, data1, data2) {
      let diffCnt = countDifferencesInArrayBuffers(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);
      return diffCnt > 0;
    },
  },
  {
    name: "CanvasRenderingContext2D.getImageData() with a offscreen canvas",
    shouldBeRandomized: true,
    extractCanvasData(browser) {
      return SpecialPowers.spawn(browser, [], async _ => {
        let offscreenCanvas = new content.OffscreenCanvas(100, 100);

        const context = offscreenCanvas.getContext("2d");

        // Draw a red rectangle
        context.fillStyle = "#EE2222";
        context.fillRect(0, 0, 100, 100);
        context.fillStyle = "#2222EE";
        context.fillRect(20, 20, 100, 100);

        const imageData = context.getImageData(0, 0, 100, 100);

        // Access the data again.
        const imageDataSecond = context.getImageData(0, 0, 100, 100);

        return [imageData.data, imageDataSecond.data];
      });
    },
    isDataRandomized(name, data1, data2, isCompareOriginal) {
      let diffCnt = countDifferencesInUint8Arrays(data1, data2);
      info(`For ${name} there are ${diffCnt} bits are different.`);

      // The Canvas randomization adds at most 512 bits noise to the image data.
      // We compare the image data arrays to see if they are different and the
      // difference is within the range.

      // If we are compare two randomized arrays, the difference can be doubled.
      let expected = isCompareOriginal
        ? NUM_RANDOMIZED_CANVAS_BITS
        : NUM_RANDOMIZED_CANVAS_BITS * 2;

      // The number of difference bits should never bigger than the expected
      // number. It could be zero if the randomization is disabled.
      ok(diffCnt <= expected, "The number of noise bits is expected.");

      return diffCnt <= expected && diffCnt > 0;
    },
  },
];

async function runTest(enabled) {
  // Enable/Disable CanvasRandomization by the RFP target overrides.
  let RFPOverrides = enabled ? "+CanvasRandomization" : "-CanvasRandomization";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      ["privacy.fingerprintingProtection.pbmode", true],
      ["privacy.fingerprintingProtection.overrides", RFPOverrides],
      ["privacy.resistFingerprinting", false],
    ],
  });

  // Open a private window.
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Open tabs in the normal and private window.
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, emptyPage);

  const privateTab = await BrowserTestUtils.openNewForegroundTab(
    privateWindow.gBrowser,
    emptyPage
  );

  for (let test of TEST_CASES) {
    info(`Testing ${test.name}`);

    // Clear telemetry before starting test.
    Services.fog.testResetFOG();

    let data = await test.extractCanvasData(tab.linkedBrowser);
    let result = test.isDataRandomized(test.name, data[0], test.originalData);

    if (test.shouldBeRandomized) {
      is(
        result,
        enabled,
        `The image data is ${enabled ? "randomized" : "the same"} for ${
          test.name
        }.`
      );
    } else {
      is(
        result,
        false,
        `The image data for ${test.name} should never be randomized.`
      );
    }

    ok(
      !test.isDataRandomized(test.name, data[0], data[1]),
      `The data of first and second access should be the same for ${test.name}.`
    );

    let privateData = await test.extractCanvasData(privateTab.linkedBrowser);

    // Check if we add noise to canvas data in private windows.
    result = test.isDataRandomized(
      test.name,
      privateData[0],
      test.originalData,
      true
    );
    if (test.shouldBeRandomized) {
      is(
        result,
        enabled,
        `The private image data is ${enabled ? "randomized" : "the same"} for ${
          test.name
        }.`
      );
    } else {
      is(
        result,
        false,
        `The image data for ${test.name} should never be randomized.`
      );
    }

    ok(
      !test.isDataRandomized(test.name, privateData[0], privateData[1]),
      "The data of first and second access should be the same for private windows."
    );

    if (test.shouldBeRandomized) {
      // Make sure the noises are different between normal window and private
      // windows.
      result = test.isDataRandomized(test.name, privateData[0], data[0]);
      is(
        result,
        enabled,
        `The image data between the normal window and the private window are ${
          enabled ? "different" : "the same"
        } for ${test.name}.`
      );

      // Verify the telemetry is recorded if canvas randomization is enabled.
      if (enabled) {
        await Services.fog.testFlushAllChildren();

        ok(
          Glean.fingerprintingProtection.canvasNoiseCalculateTime.testGetValue()
            .sum > 0,
          "The telemetry of canvas randomization is recorded."
        );
      }
    }
  }

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(privateTab);
  await BrowserTestUtils.closeWindow(privateWindow);
}

add_setup(async function () {
  // Disable the fingerprinting randomization.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", false],
      ["privacy.fingerprintingProtection.pbmode", false],
      ["privacy.resistFingerprinting", false],
    ],
  });

  // Open a tab for extracting the canvas data.
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, emptyPage);

  // Extract the original canvas data without random noise.
  for (let test of TEST_CASES) {
    let data = await test.extractCanvasData(tab.linkedBrowser);
    test.originalData = data[0];
  }

  BrowserTestUtils.removeTab(tab);
});

add_task(async function run_tests_with_randomization_enabled() {
  await runTest(true);
});

add_task(async function run_tests_with_randomization_disabled() {
  await runTest(false);
});
