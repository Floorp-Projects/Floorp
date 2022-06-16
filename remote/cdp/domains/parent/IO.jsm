/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["IO", "streamRegistry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/cdp/domains/Domain.jsm"
);
const { StreamRegistry } = ChromeUtils.import(
  "chrome://remote/content/cdp/StreamRegistry.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  OS: "resource://gre/modules/osfile.jsm",
});

const DEFAULT_CHUNK_SIZE = 10 * 1024 * 1024;

// Global singleton for managing open streams
const streamRegistry = new StreamRegistry();

class IO extends Domain {
  // commands

  /**
   * Close the stream, discard any temporary backing storage.
   *
   * @param {Object} options
   * @param {string} options.handle
   *     Handle of the stream to close.
   */
  async close(options = {}) {
    const { handle } = options;

    if (typeof handle != "string") {
      throw new TypeError(`handle: string value expected`);
    }

    await streamRegistry.remove(handle);
  }

  /**
   * Read a chunk of the stream.
   *
   * @param {Object} options
   * @param {string} options.handle
   *     Handle of the stream to read.
   * @param {number=} options.offset
   *     Seek to the specified offset before reading. If not specificed,
   *     proceed with offset following the last read.
   *     Some types of streams may only support sequential reads.
   * @param {number=} options.size
   *     Maximum number of bytes to read (left upon the agent
   *     discretion if not specified).
   *
   * @return {string, boolean, boolean}
   *     Data that were read, including flags for base64-encoded, and end-of-file reached.
   */
  async read(options = {}) {
    const { handle, offset, size } = options;

    if (typeof handle != "string") {
      throw new TypeError(`handle: string value expected`);
    }

    const stream = streamRegistry.get(handle);
    const fileInfo = await stream.stat();

    if (typeof offset != "undefined") {
      if (typeof offset != "number") {
        throw new TypeError(`offset: integer value expected`);
      }

      // To keep compatibility with Chrome clip invalid offsets
      const seekTo = Math.max(0, Math.min(offset, fileInfo.size));
      await stream.setPosition(seekTo, lazy.OS.File.POS_START);
    }

    const curPos = await stream.getPosition();
    const remainingBytes = fileInfo.size - curPos;

    let chunkSize;
    if (typeof size != "undefined") {
      if (typeof size != "number") {
        throw new TypeError(`size: integer value expected`);
      }

      // Chromium currently crashes for negative sizes (https://bit.ly/2P6h0Fv),
      // but might behave similar to the offset and clip invalid values
      chunkSize = Math.max(0, Math.min(size, remainingBytes));
    } else {
      chunkSize = Math.min(DEFAULT_CHUNK_SIZE, remainingBytes);
    }

    const bytes = await stream.read(chunkSize);
    // Each UCS2 character has an upper byte of 0 and a lower byte matching
    // the binary data. Using a loop here prevents us from hitting the browser's
    // internal `arguments.length` limit.
    const ARGS_MAX = 262144;
    const stringData = [];
    for (let i = 0; i < bytes.length; i += ARGS_MAX) {
      let argsChunk = Math.min(bytes.length, i + ARGS_MAX);
      stringData.push(
        String.fromCharCode.apply(null, bytes.slice(i, argsChunk))
      );
    }
    const data = btoa(stringData.join(""));

    return {
      data,
      base64Encoded: true,
      eof: remainingBytes - bytes.length == 0,
    };
  }
}
