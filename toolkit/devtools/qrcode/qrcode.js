/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { Encoder, QRRSBlock, QRErrorCorrectLevel } = require("./encoder/index");

/**
 * There are many "versions" of QR codes, which describes how many dots appear
 * in the resulting image, thus limiting the amount of data that can be
 * represented.
 *
 * The encoder used here allows for versions 1 - 10 (more dots for larger
 * versions).
 *
 * It expects you to pick a version large enough to contain your message.  Here
 * we search for the mimimum version based on the message length.
 * @param string message
 *        Text to encode
 * @param string quality
 *        Quality level: L, M, Q, H
 * @return integer
 */
exports.findMinimumVersion = function(message, quality) {
  let msgLength = message.length;
  let qualityLevel = QRErrorCorrectLevel[quality];
  for (let version = 1; version <= 10; version++) {
    let rsBlocks = QRRSBlock.getRSBlocks(version, qualityLevel);
    let maxLength = rsBlocks.reduce((prev, block) => {
      return prev + block.dataCount;
    }, 0);
    // Remove two bytes to fit header info
    maxLength -= 2;
    if (msgLength <= maxLength) {
      return version;
    }
  }
  throw new Error("Message too large");
};

/**
 * Simple wrapper around the underlying encoder's API.
 * @param string  message
 *        Text to encode
 * @param string  quality (optional)
          Quality level: L, M, Q, H
 * @param integer version (optional)
 *        QR code "version" large enough to contain the message
 * @return object with the following fields:
 *   * src:    an image encoded a data URI
 *   * height: image height
 *   * width:  image width
 */
exports.encodeToDataURI = function(message, quality, version) {
  quality = quality || "H";
  version = version || exports.findMinimumVersion(message, quality);
  let encoder = new Encoder(version, quality);
  encoder.addData(message);
  encoder.make();
  return encoder.createImgData();
};
