/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global runTestInternal b:true*/

function runTest(testName, canvasType, videoUri) {
  const canvas = document.createElement("canvas");
  canvas.width = 1920;
  canvas.height = 1080;
  document.body.appendChild(canvas);

  const offscreenCanvas = canvas.transferControlToOffscreen();

  return new Promise((resolve, reject) => {
    try {
      runTestInternal(
        testName,
        canvasType,
        offscreenCanvas,
        /* isWorker */ false,
        videoUri,
        resolve,
        reject
      );
    } catch (e) {
      reject(e);
    }
  })
    .then(result => {
      let name =
        result.testName +
        " Mean time across " +
        result.totalFrames +
        " frames: ";
      let value = result.elapsed / result.totalFrames;
      let msg = name + value + "\n";
      dump("[talos offscreen-canvas-webcodecs result] " + msg);

      if (window.tpRecordTime) {
        // Within talos - report the results
        tpRecordTime(value, 0, name);
      } else {
        alert(msg);
      }
    })
    .catch(e => {
      let msg = "caught error " + e;
      dump("[talos offscreen-canvas-webcodecs result] " + msg);
      if (window.tpRecordTime) {
        // Within talos - report the results
        tpRecordTime("", 0, "");
      } else {
        alert(msg);
      }
    });
}
