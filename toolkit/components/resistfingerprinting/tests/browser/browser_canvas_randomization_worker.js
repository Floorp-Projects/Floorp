/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const emptyPage =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

/**
 * Bug 1816189 - Testing canvas randomization on canvas data extraction in
 * workers.
 *
 * In the test, we create offscreen canvas in workers and test if the extracted
 * canvas data is altered because of the canvas randomization.
 */

var TEST_CASES = [
  {
    name: "CanvasRenderingContext2D.getImageData() with a offscreen canvas",
    extractCanvasData: async _ => {
      let offscreenCanvas = new OffscreenCanvas(100, 100);

      const context = offscreenCanvas.getContext("2d");

      context.fillStyle = "#EE2222";
      context.fillRect(0, 0, 100, 100);
      context.fillStyle = "#2222EE";
      context.fillRect(20, 20, 100, 100);

      const imageData = context.getImageData(0, 0, 100, 100);

      const imageDataSecond = context.getImageData(0, 0, 100, 100);

      return [imageData.data, imageDataSecond.data];
    },
    isDataRandomized: isDataRandomizedFuzzy,
  },
  {
    name: "OffscreenCanvas.convertToBlob() with a 2d context",
    extractCanvasData: async _ => {
      let offscreenCanvas = new OffscreenCanvas(100, 100);

      const context = offscreenCanvas.getContext("2d");

      // Draw a red rectangle
      context.fillStyle = "#EE2222";
      context.fillRect(0, 0, 100, 100);
      context.fillStyle = "#2222EE";
      context.fillRect(20, 20, 100, 100);

      let blob = await offscreenCanvas.convertToBlob();

      let data = await new Promise(resolve => {
        let fileReader = new FileReader();
        fileReader.onload = () => {
          resolve(fileReader.result);
        };
        fileReader.readAsArrayBuffer(blob);
      });

      let blobSecond = await offscreenCanvas.convertToBlob();

      let dataSecond = await new Promise(resolve => {
        let fileReader = new FileReader();
        fileReader.onload = () => {
          resolve(fileReader.result);
        };
        fileReader.readAsArrayBuffer(blobSecond);
      });

      return [data, dataSecond];
    },
    isDataRandomized: isDataRandomizedGreaterThanZero,
  },
  {
    name: "OffscreenCanvas.convertToBlob() with a webgl context",
    extractCanvasData: async _ => {
      let offscreenCanvas = new OffscreenCanvas(100, 100);

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

      let data = await new Promise(resolve => {
        let fileReader = new FileReader();
        fileReader.onload = () => {
          resolve(fileReader.result);
        };
        fileReader.readAsArrayBuffer(blob);
      });

      let blobSecond = await offscreenCanvas.convertToBlob();

      let dataSecond = await new Promise(resolve => {
        let fileReader = new FileReader();
        fileReader.onload = () => {
          resolve(fileReader.result);
        };
        fileReader.readAsArrayBuffer(blobSecond);
      });

      return [data, dataSecond];
    },
    isDataRandomized: isDataRandomizedGreaterThanZero,
  },
  {
    name: "OffscreenCanvas.convertToBlob() with a bitmaprenderer context",
    extractCanvasData: async _ => {
      let offscreenCanvas = new OffscreenCanvas(100, 100);

      const context = offscreenCanvas.getContext("2d");

      // Draw a red rectangle
      context.fillStyle = "#EE2222";
      context.fillRect(0, 0, 100, 100);
      context.fillStyle = "#2222EE";
      context.fillRect(20, 20, 100, 100);

      const bitmapCanvas = new OffscreenCanvas(100, 100);

      let bitmap = await createImageBitmap(offscreenCanvas);
      const bitmapContext = bitmapCanvas.getContext("bitmaprenderer");
      bitmapContext.transferFromImageBitmap(bitmap);

      let blob = await bitmapCanvas.convertToBlob();

      let data = await new Promise(resolve => {
        let fileReader = new FileReader();
        fileReader.onload = () => {
          resolve(fileReader.result);
        };
        fileReader.readAsArrayBuffer(blob);
      });

      let blobSecond = await bitmapCanvas.convertToBlob();

      let dataSecond = await new Promise(resolve => {
        let fileReader = new FileReader();
        fileReader.onload = () => {
          resolve(fileReader.result);
        };
        fileReader.readAsArrayBuffer(blobSecond);
      });

      return [data, dataSecond];
    },
    isDataRandomized: isDataRandomizedGreaterThanZero,
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
    info(`Testing ${test.name} in the worker`);

    // Clear telemetry before starting test.
    Services.fog.testResetFOG();

    let data = await await runFunctionInWorker(
      tab.linkedBrowser,
      test.extractCanvasData
    );

    let result = test.isDataRandomized(test.name, data[0], test.originalData);
    is(
      result,
      enabled,
      `The image data for for '${test.name}' is ${
        enabled ? "randomized" : "the same"
      }.`
    );

    ok(
      !test.isDataRandomized(test.name, data[0], data[1]),
      `The data for '${test.name}' of first and second access should be the same.`
    );

    let privateData = await await runFunctionInWorker(
      privateTab.linkedBrowser,
      test.extractCanvasData
    );

    // Check if we add noise to canvas data in private windows.
    result = test.isDataRandomized(
      test.name,
      privateData[0],
      test.originalData,
      true
    );
    is(
      result,
      enabled,
      `The private image data for '${test.name}' is ${
        enabled ? "randomized" : "the same"
      }.`
    );

    ok(
      !test.isDataRandomized(test.name, privateData[0], privateData[1]),
      `"The data for '${test.name}' of first and second access should be the same.`
    );

    // Make sure the noises are different between normal window and private
    // windows.
    result = test.isDataRandomized(test.name, privateData[0], data[0]);
    is(
      result,
      enabled,
      `The image data for '${
        test.name
      }' between the normal window and the private window are ${
        enabled ? "different" : "the same"
      }.`
    );
  }

  // Verify the telemetry is recorded if canvas randomization is enabled.
  if (enabled) {
    await Services.fog.testFlushAllChildren();

    Assert.greater(
      Glean.fingerprintingProtection.canvasNoiseCalculateTime.testGetValue()
        .sum,
      0,
      "The telemetry of canvas randomization is recorded."
    );
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

  // Open a tab for extracting the canvas data in the worker.
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, emptyPage);

  // Extract the original canvas data without random noise.
  for (let test of TEST_CASES) {
    let data = await runFunctionInWorker(
      tab.linkedBrowser,
      test.extractCanvasData
    );
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
