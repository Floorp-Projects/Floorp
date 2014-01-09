/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

if (typeof Components != "undefined") {
  throw new Error("This file is meant to be loaded in a worker");
}
if (!module || !exports) {
  throw new Error("Please load this module with require()");
}

let SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
let libxul = ctypes.open(SharedAll.Constants.Path.libxul);
let declareLazyFII = SharedAll.declareLazyFFI;
let Type = SharedAll.Type;

let Primitives = {};

declareLazyFII(Primitives, "compress", libxul,
  "workerlz4_compress",
  null,
  /*return*/ Type.size_t,
  /*const source*/ Type.void_t.in_ptr,
  /*inputSize*/ Type.size_t,
  /*dest*/ Type.void_t.out_ptr
);

declareLazyFII(Primitives, "decompress", libxul,
  "workerlz4_decompress",
  null,
  /*return*/ Type.int,
  /*const source*/ Type.void_t.in_ptr,
  /*inputSize*/ Type.size_t,
  /*dest*/ Type.void_t.out_ptr,
  /*maxOutputSize*/ Type.size_t,
  /*actualOutputSize*/ Type.size_t.out_ptr
);

declareLazyFII(Primitives, "maxCompressedSize", libxul,
  "workerlz4_maxCompressedSize",
  null,
  /*return*/ Type.size_t,
  /*inputSize*/ Type.size_t
);

module.exports = {
  get compress() {
    return Primitives.compress;
  },
  get decompress() {
    return Primitives.decompress;
  },
  get maxCompressedSize() {
    return Primitives.maxCompressedSize;
  }
};
