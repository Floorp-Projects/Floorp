/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global MP4Demuxer b:true*/

const kTotalFrames = 100;

let start;
let frameCount;

function runTestInternal(
  testName,
  canvasType,
  canvas,
  isWorker,
  videoUri,
  resolve,
  reject
) {
  const runContext = isWorker ? "worker" : "main";
  dump(
    "[" +
      canvasType +
      " webcodecs " +
      runContext +
      "] load video '" +
      videoUri +
      "'\n"
  );

  let drawImage;
  if (canvasType == "2d") {
    const ctx = canvas.getContext("2d");
    drawImage = function (frame) {
      ctx.drawImage(frame, 0, 0);
    };
  } else if (canvasType == "webgl") {
    const gl = canvas.getContext("webgl");
    gl.bindTexture(gl.TEXTURE_2D, gl.createTexture());
    drawImage = function (frame) {
      gl.texImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RGBA,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        frame
      );
    };
  } else {
    dump("[" + canvasType + " webcodecs " + runContext + "] unknown type\n");
    reject("unknown type " + canvasType);
    return;
  }

  frameCount = 0;

  const decoder = new VideoDecoder({
    output(frame) {
      if (frameCount == 0) {
        start = performance.now();
      }
      ++frameCount;

      dump(
        "[" +
          canvasType +
          " webcodecs " +
          runContext +
          "] draw frame " +
          frameCount +
          "\n"
      );
      drawImage(frame);
      frame.close();

      if (frameCount == kTotalFrames) {
        var elapsed = performance.now() - start;
        setTimeout(() => {
          resolve({ testName, elapsed, totalFrames: kTotalFrames });
        }, 0);

        dump(
          "[" + canvasType + " webcodecs " + runContext + "] close decoder\n"
        );
        decoder.close();
      }
    },
    error(e) {
      dump(
        "[" +
          canvasType +
          " webcodecs " +
          runContext +
          "] decoder error " +
          e +
          "\n"
      );
      decoder.close();
      reject(e);
    },
  });

  new MP4Demuxer(videoUri, {
    onConfig(config) {
      dump(
        "[" + canvasType + " webcodecs " + runContext + "] demuxer config\n"
      );
      decoder.configure(config);
    },
    onChunk(chunk) {
      dump("[" + canvasType + " webcodecs " + runContext + "] demuxer chunk\n");
      decoder.decode(chunk);
    },
    setStatus(msg) {
      dump(
        "[" +
          canvasType +
          " webcodecs " +
          runContext +
          "] demuxer status - " +
          msg +
          "\n"
      );
    },
  });
}
