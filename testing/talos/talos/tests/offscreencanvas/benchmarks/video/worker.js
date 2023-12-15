/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

importScripts("mp4box.all.min.js");
importScripts("demuxer_mp4.js");
importScripts("shared.js");

self.onmessage = e => {
  const resolve = result => {
    self.postMessage(result);
  };
  const reject = errorMessage => {
    self.postMessage({ errorMessage });
  };

  try {
    runTestInternal(
      e.data.testName,
      e.data.canvasType,
      e.data.offscreenCanvas,
      /* isWorker */ true,
      e.data.videoUri,
      resolve,
      reject
    );
  } catch (e) {
    reject(e);
  }
};
