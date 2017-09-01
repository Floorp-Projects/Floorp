/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Native (xpcom) implementation of key OS.File functions
 */

"use strict";

this.EXPORTED_SYMBOLS = ["read"];

var {results: Cr, utils: Cu, interfaces: Ci} = Components;

var SharedAll = Cu.import("resource://gre/modules/osfile/osfile_shared_allthreads.jsm", {});

var SysAll = {};
if (SharedAll.Constants.Win) {
  Cu.import("resource://gre/modules/osfile/osfile_win_allthreads.jsm", SysAll);
} else if (SharedAll.Constants.libc) {
  Cu.import("resource://gre/modules/osfile/osfile_unix_allthreads.jsm", SysAll);
} else {
  throw new Error("I am neither under Windows nor under a Posix system");
}
var {XPCOMUtils} = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

/**
 * The native service holding the implementation of the functions.
 */
XPCOMUtils.defineLazyServiceGetter(this,
  "Internals",
  "@mozilla.org/toolkit/osfile/native-internals;1",
  "nsINativeOSFileInternalsService");

/**
 * Native implementation of OS.File.read
 *
 * This implementation does not handle option |compression|.
 */
this.read = function(path, options = {}) {
  // Sanity check on types of options
  if ("encoding" in options && typeof options.encoding != "string") {
    return Promise.reject(new TypeError("Invalid type for option encoding"));
  }
  if ("compression" in options && typeof options.compression != "string") {
    return Promise.reject(new TypeError("Invalid type for option compression"));
  }
  if ("bytes" in options && typeof options.bytes != "number") {
    return Promise.reject(new TypeError("Invalid type for option bytes"));
  }

  return new Promise((resolve, reject) => {
    Internals.read(path,
      options,
      function onSuccess(success) {
        success.QueryInterface(Ci.nsINativeOSFileResult);
        if ("outExecutionDuration" in options) {
          options.outExecutionDuration =
            success.executionDurationMS +
            (options.outExecutionDuration || 0);
        }
        resolve(success.result);
      },
      function onError(operation, oserror) {
        reject(new SysAll.Error(operation, oserror, path));
      }
    );
  });
};
