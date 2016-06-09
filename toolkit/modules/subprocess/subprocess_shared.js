/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported Library, SubprocessConstants */

if (!ArrayBuffer.transfer) {
  /**
   * @see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer/transfer
   *
   * @param {ArrayBuffer} buffer
   * @param {integer} [size = buffer.byteLength]
   * @returns {ArrayBuffer}
   */
  ArrayBuffer.transfer = function(buffer, size = buffer.byteLength) {
    let u8out = new Uint8Array(size);
    let u8buffer = new Uint8Array(buffer, 0, Math.min(size, buffer.byteLength));

    u8out.set(u8buffer);

    return u8out.buffer;
  };
}

var libraries = {};

class Library {
  constructor(name, names, definitions) {
    if (name in libraries) {
      return libraries[name];
    }

    for (let name of names) {
      try {
        if (!this.library) {
          this.library = ctypes.open(name);
        }
      } catch (e) {
        // Ignore errors until we've tried all the options.
      }
    }
    if (!this.library) {
      throw new Error("Could not load libc");
    }

    libraries[name] = this;

    for (let symbol of Object.keys(definitions)) {
      this.declare(symbol, ...definitions[symbol]);
    }
  }

  declare(name, ...args) {
    Object.defineProperty(this, name, {
      configurable: true,
      get() {
        Object.defineProperty(this, name, {
          configurable: true,
          value: this.library.declare(name, ...args),
        });

        return this[name];
      },
    });
  }
}

/**
 * Holds constants which apply to various Subprocess operations.
 * @namespace
 * @lends Subprocess
 */
const SubprocessConstants = {
  /**
   * @property {integer} ERROR_END_OF_FILE
   *           The operation failed because the end of the file was reached.
   *           @constant
   */
  ERROR_END_OF_FILE: 0xff7a0001,
  /**
   * @property {integer} ERROR_INVALID_JSON
   *           The operation failed because an invalid JSON was encountered.
   *           @constant
   */
  ERROR_INVALID_JSON: 0xff7a0002,
  /**
   * @property {integer} ERROR_BAD_EXECUTABLE
   *           The operation failed because the given file did not exist, or
   *           could not be executed.
   *           @constant
   */
  ERROR_BAD_EXECUTABLE: 0xff7a0003,
};

Object.freeze(SubprocessConstants);
