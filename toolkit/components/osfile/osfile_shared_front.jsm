/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code shared by OS.File front-ends.
 *
 * This code is meant to be included by another library. It is also meant to
 * be executed only on a worker thread.
 */

if (typeof Components != "undefined") {
  throw new Error("osfile_shared_front.jsm cannot be used from the main thread");
}
(function(exports) {

/**
 * Code shared by implementations of File.
 *
 * @param {*} fd An OS-specific file handle.
 * @constructor
 */
let AbstractFile = function AbstractFile(fd) {
  this._fd = fd;
};

AbstractFile.prototype = {
  /**
   * Return the file handle.
   *
   * @throw OS.File.Error if the file has been closed.
   */
  get fd() {
    if (this._fd) {
      return this._fd;
    }
    throw OS.File.Error.closed();
  }
};


/**
 * Utility function shared by implementations of |OS.File.open|:
 * extract read/write/trunc/create/existing flags from a |mode|
 * object.
 *
 * @param {*=} mode An object that may contain fields |read|,
 * |write|, |truncate|, |create|, |existing|. These fields
 * are interpreted only if true-ish.
 * @return {{read:bool, write:bool, trunc:bool, create:bool,
 * existing:bool}} an object recapitulating the options set
 * by |mode|.
 * @throws {TypeError} If |mode| contains other fields, or
 * if it contains both |create| and |truncate|, or |create|
 * and |existing|.
 */
AbstractFile.normalizeOpenMode = function normalizeOpenMode(mode) {
  let result = {
    read: false,
    write: false,
    trunc: false,
    create: false,
    existing: false
  };
  for (let key in mode) {
    if (!mode[key]) continue; // Only interpret true-ish keys
    switch (key) {
    case "read":
      result.read = true;
      break;
    case "write":
      result.write = true;
      break;
    case "truncate": // fallthrough
    case "trunc":
      result.trunc = true;
      result.write = true;
      break;
    case "create":
      result.create = true;
      result.write = true;
      break;
    case "existing": // fallthrough
    case "exist":
      result.existing = true;
      break;
    default:
      throw new TypeError("Mode " + key + " not understood");
    }
  }
  // Reject opposite modes
  if (result.existing && result.create) {
    throw new TypeError("Cannot specify both existing:true and create:true");
  }
  if (result.trunc && result.create) {
    throw new TypeError("Cannot specify both trunc:true and create:true");
  }
  // Handle read/write
  if (!result.write) {
    result.read = true;
  }
  return result;
};

   exports.OS.Shared.AbstractFile = AbstractFile;
})(this);