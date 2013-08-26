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

exports.OS = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm").OS;

let LOG = exports.OS.Shared.LOG.bind(OS.Shared, "Shared front-end");

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
  },
  /**
   * Read bytes from this file to a new buffer.
   *
   * @param {number=} bytes If unspecified, read all the remaining bytes from
   * this file. If specified, read |bytes| bytes, or less if the file does not
   * contain that many bytes.
   * @return {Uint8Array} An array containing the bytes read.
   */
  read: function read(bytes) {
    if (bytes == null) {
      bytes = this.stat().size;
    }
    let buffer = new Uint8Array(bytes);
    let size = this.readTo(buffer, {bytes: bytes});
    if (size == bytes) {
      return buffer;
    } else {
      return buffer.subarray(0, size);
    }
  },

  /**
   * Read bytes from this file to an existing buffer.
   *
   * Note that, by default, this function may perform several I/O
   * operations to ensure that the buffer is as full as possible.
   *
   * @param {Typed Array | C pointer} buffer The buffer in which to
   * store the bytes. The buffer must be large enough to
   * accomodate |bytes| bytes.
   * @param {*=} options Optionally, an object that may contain the
   * following fields:
   * - {number} bytes The number of |bytes| to write from the buffer. If
   * unspecified, this is |buffer.byteLength|. Note that |bytes| is required
   * if |buffer| is a C pointer.
   *
   * @return {number} The number of bytes actually read, which may be
   * less than |bytes| if the file did not contain that many bytes left.
   */
  readTo: function readTo(buffer, options = {}) {
    let {ptr, bytes} = AbstractFile.normalizeToPointer(buffer, options.bytes);
    let pos = 0;
    while (pos < bytes) {
      let chunkSize = this._read(ptr, bytes - pos, options);
      if (chunkSize == 0) {
        break;
      }
      pos += chunkSize;
      ptr = exports.OS.Shared.offsetBy(ptr, chunkSize);
    }

    return pos;
  },

  /**
   * Write bytes from a buffer to this file.
   *
   * Note that, by default, this function may perform several I/O
   * operations to ensure that the buffer is fully written.
   *
   * @param {Typed array | C pointer} buffer The buffer in which the
   * the bytes are stored. The buffer must be large enough to
   * accomodate |bytes| bytes.
   * @param {*=} options Optionally, an object that may contain the
   * following fields:
   * - {number} bytes The number of |bytes| to write from the buffer. If
   * unspecified, this is |buffer.byteLength|. Note that |bytes| is required
   * if |buffer| is a C pointer.
   *
   * @return {number} The number of bytes actually written.
   */
  write: function write(buffer, options = {}) {

    let {ptr, bytes} = AbstractFile.normalizeToPointer(buffer, options.bytes);

    let pos = 0;
    while (pos < bytes) {
      let chunkSize = this._write(ptr, bytes - pos, options);
      pos += chunkSize;
      ptr = exports.OS.Shared.offsetBy(ptr, chunkSize);
    }
    return pos;
  }
};

/**
 * Utility function used to normalize a Typed Array or C
 * pointer into a uint8_t C pointer.
 *
 * Future versions might extend this to other data structures.
 *
 * @param {Typed array | C pointer} candidate The buffer. If
 * a C pointer, it must be non-null.
 * @param {number} bytes The number of bytes that |candidate| should contain.
 * Used for sanity checking if the size of |candidate| can be determined.
 *
 * @return {ptr:{C pointer}, bytes:number} A C pointer of type uint8_t,
 * corresponding to the start of |candidate|.
 */
AbstractFile.normalizeToPointer = function normalizeToPointer(candidate, bytes) {
  if (!candidate) {
    throw new TypeError("Expecting  a Typed Array or a C pointer");
  }
  let ptr;
  if ("isNull" in candidate) {
    if (candidate.isNull()) {
      throw new TypeError("Expecting a non-null pointer");
    }
    ptr = exports.OS.Shared.Type.uint8_t.out_ptr.cast(candidate);
    if (bytes == null) {
      throw new TypeError("C pointer missing bytes indication.");
    }
  } else if (exports.OS.Shared.isTypedArray(candidate)) {
    // Typed Array
    ptr = exports.OS.Shared.Type.uint8_t.out_ptr.implementation(candidate.buffer);
    if (bytes == null) {
      bytes = candidate.byteLength;
    } else if (candidate.byteLength < bytes) {
      throw new TypeError("Buffer is too short. I need at least " +
                         bytes +
                         " bytes but I have only " +
                         candidate.byteLength +
                          "bytes");
    }
  } else {
    throw new TypeError("Expecting  a Typed Array or a C pointer");
  }
  return {ptr: ptr, bytes: bytes};
};

/**
 * Code shared by iterators.
 */
AbstractFile.AbstractIterator = function AbstractIterator() {
};
AbstractFile.AbstractIterator.prototype = {
  /**
   * Allow iterating with |for|
   */
  __iterator__: function __iterator__() {
    return this;
  },
  /**
   * Apply a function to all elements of the directory sequentially.
   *
   * @param {Function} cb This function will be applied to all entries
   * of the directory. It receives as arguments
   *  - the OS.File.Entry corresponding to the entry;
   *  - the index of the entry in the enumeration;
   *  - the iterator itself - calling |close| on the iterator stops
   *   the loop.
   */
  forEach: function forEach(cb) {
    let index = 0;
    for (let entry in this) {
      cb(entry, index++, this);
    }
  },
  /**
   * Return several entries at once.
   *
   * Entries are returned in the same order as a walk with |forEach| or
   * |for(...)|.
   *
   * @param {number=} length If specified, the number of entries
   * to return. If unspecified, return all remaining entries.
   * @return {Array} An array containing the next |length| entries, or
   * less if the iteration contains less than |length| entries left.
   */
  nextBatch: function nextBatch(length) {
    let array = [];
    let i = 0;
    for (let entry in this) {
      array.push(entry);
      if (++i >= length) {
        return array;
      }
    }
    return array;
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

/**
 * Return the contents of a file.
 *
 * @param {string} path The path to the file.
 * @param {number=} bytes Optionally, an upper bound to the number of bytes
 * to read.
 *
 * @return {Uint8Array} A buffer holding the bytes
 * and the number of bytes read from the file.
 */
AbstractFile.read = function read(path, bytes) {
  let file = exports.OS.File.open(path);
  try {
    return file.read(bytes);
  } finally {
    file.close();
  }
};

/**
 * Write a file, atomically.
 *
 * By opposition to a regular |write|, this operation ensures that,
 * until the contents are fully written, the destination file is
 * not modified.
 *
 * Limitation: In a few extreme cases (hardware failure during the
 * write, user unplugging disk during the write, etc.), data may be
 * corrupted. If your data is user-critical (e.g. preferences,
 * application data, etc.), you may wish to consider adding options
 * |tmpPath| and/or |flush| to reduce the likelihood of corruption, as
 * detailed below. Note that no combination of options can be
 * guaranteed to totally eliminate the risk of corruption.
 *
 * @param {string} path The path of the file to modify.
 * @param {Typed Array | C pointer} buffer A buffer containing the bytes to write.
 * @param {*=} options Optionally, an object determining the behavior
 * of this function. This object may contain the following fields:
 * - {number} bytes The number of bytes to write. If unspecified,
 * |buffer.byteLength|. Required if |buffer| is a C pointer.
 * - {string} tmpPath If |null| or unspecified, write all data directly
 * to |path|. If specified, write all data to a temporary file called
 * |tmpPath| and, once this write is complete, rename the file to
 * replace |path|. Performing this additional operation is a little
 * slower but also a little safer.
 * - {bool} noOverwrite - If set, this function will fail if a file already
 * exists at |path|.
 * - {bool} flush - If |false| or unspecified, return immediately once the
 * write is complete. If |true|, before writing, force the operating system
 * to write its internal disk buffers to the disk. This is considerably slower
 * (not just for the application but for the whole system) but also safer:
 * if the system shuts down improperly (typically due to a kernel freeze
 * or a power failure) or if the device is disconnected before the buffer
 * is flushed, the file has more chances of not being corrupted.
 *
 * @return {number} The number of bytes actually written.
 */
AbstractFile.writeAtomic =
     function writeAtomic(path, buffer, options = {}) {

  // Verify that path is defined and of the correct type
  if (typeof path != "string" || path == "") {
    throw new TypeError("File path should be a (non-empty) string");
  }
  let noOverwrite = options.noOverwrite;
  if (noOverwrite && OS.File.exists(path)) {
    throw OS.File.Error.exists("writeAtomic");
  }

  if (typeof buffer == "string") {
    // Normalize buffer to a C buffer by encoding it
    let encoding = options.encoding || "utf-8";
    buffer = new TextEncoder(encoding).encode(buffer);
  }

  let bytesWritten = 0;

  if (!options.tmpPath) {
    // Just write, without any renaming trick
    let dest = OS.File.open(path, {write: true, truncate: true});
    try {
      bytesWritten = dest.write(buffer, options);
      if (options.flush) {
        dest.flush();
      }
    } finally {
      dest.close();
    }
    return bytesWritten;
  }

  let tmpFile = OS.File.open(options.tmpPath, {write: true, truncate: true});
  try {
    bytesWritten = tmpFile.write(buffer, options);
    if (options.flush) {
      tmpFile.flush();
    }
  } catch (x) {
    OS.File.remove(options.tmpPath);
    throw x;
  } finally {
    tmpFile.close();
  }

  OS.File.move(options.tmpPath, path, {noCopy: true});
  return bytesWritten;
};

   exports.OS.Shared.AbstractFile = AbstractFile;
})(this);
