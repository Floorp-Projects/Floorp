/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Primitives = {};

var SharedAll;
if (typeof Components != "undefined") {
  let Cu = Components.utils;
  SharedAll = {};
  Cu.import("resource://gre/modules/osfile/osfile_shared_allthreads.jsm", SharedAll);

  this.EXPORTED_SYMBOLS = [
    "Primitives"
  ];
  this.Primitives = Primitives;
  this.exports = {};
} else if (typeof module != "undefined" && typeof require != "undefined") {
  SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
} else {
  throw new Error("Please load this module with Component.utils.import or with require()");
}

var libxul = new SharedAll.Library("libxul", SharedAll.Constants.Path.libxul);
var Type = SharedAll.Type;

libxul.declareLazyFFI(Primitives, "compress",
  "workerlz4_compress",
  null,
  /* return*/ Type.size_t,
  /* const source*/ Type.void_t.in_ptr,
  /* inputSize*/ Type.size_t,
  /* dest*/ Type.void_t.out_ptr
);

libxul.declareLazyFFI(Primitives, "decompress",
  "workerlz4_decompress",
  null,
  /* return*/ Type.int,
  /* const source*/ Type.void_t.in_ptr,
  /* inputSize*/ Type.size_t,
  /* dest*/ Type.void_t.out_ptr,
  /* maxOutputSize*/ Type.size_t,
  /* actualOutputSize*/ Type.size_t.out_ptr
);

libxul.declareLazyFFI(Primitives, "maxCompressedSize",
  "workerlz4_maxCompressedSize",
  null,
  /* return*/ Type.size_t,
  /* inputSize*/ Type.size_t
);

if (typeof module != "undefined") {
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
}
