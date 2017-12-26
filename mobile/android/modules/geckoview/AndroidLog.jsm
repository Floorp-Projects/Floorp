/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Native Android logging for JavaScript.  Lets you specify a priority and tag
 * in addition to the message being logged.  Resembles the android.util.Log API
 * <http://developer.android.com/reference/android/util/Log.html>.
 *
 * // Import it as a JSM:
 * let Log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog;
 *
 * // Or require it in a chrome worker:
 * importScripts("resource://gre/modules/workers/require.js");
 * let Log = require("resource://gre/modules/AndroidLog.jsm");
 *
 * // Use Log.i, Log.v, Log.d, Log.w, and Log.e to log verbose, debug, info,
 * // warning, and error messages, respectively.
 * Log.v("MyModule", "This is a verbose message.");
 * Log.d("MyModule", "This is a debug message.");
 * Log.i("MyModule", "This is an info message.");
 * Log.w("MyModule", "This is a warning message.");
 * Log.e("MyModule", "This is an error message.");
 *
 * // Bind a function with a tag to replace a bespoke dump/log/debug function:
 * let debug = Log.d.bind(null, "MyModule");
 * debug("This is a debug message.");
 * // Outputs "D/GeckoMyModule(#####): This is a debug message."
 *
 * // Or "bind" the module object to a tag to automatically tag messages:
 * Log = Log.bind("MyModule");
 * Log.d("This is a debug message.");
 * // Outputs "D/GeckoMyModule(#####): This is a debug message."
 *
 * Note: the module automatically prepends "Gecko" to the tag you specify,
 * since all tags used by Fennec code should start with that string; and it
 * truncates tags longer than MAX_TAG_LENGTH characters (not including "Gecko").
 */

if (typeof Components != "undefined") {
  // Specify exported symbols for JSM module loader.
  this.EXPORTED_SYMBOLS = ["AndroidLog"];
  Components.utils.import("resource://gre/modules/ctypes.jsm");
}

// From <https://android.googlesource.com/platform/system/core/+/master/include/android/log.h>.
const ANDROID_LOG_VERBOSE = 2;
const ANDROID_LOG_DEBUG = 3;
const ANDROID_LOG_INFO = 4;
const ANDROID_LOG_WARN = 5;
const ANDROID_LOG_ERROR = 6;

// android.util.Log.isLoggable throws IllegalArgumentException if a tag length
// exceeds 23 characters, and we prepend five characters ("Gecko") to every tag,
// so we truncate tags exceeding 18 characters (although __android_log_write
// itself and other android.util.Log methods don't seem to mind longer tags).
const MAX_TAG_LENGTH = 18;

var liblog = ctypes.open("liblog.so"); // /system/lib/liblog.so
var __android_log_write = liblog.declare("__android_log_write",
                                         ctypes.default_abi,
                                         ctypes.int, // return value: num bytes logged
                                         ctypes.int, // priority (ANDROID_LOG_* constant)
                                         ctypes.char.ptr, // tag
                                         ctypes.char.ptr); // message

var AndroidLog = {
  MAX_TAG_LENGTH: MAX_TAG_LENGTH,
  v: (tag, msg) => __android_log_write(ANDROID_LOG_VERBOSE, "Gecko" + tag.substring(0, MAX_TAG_LENGTH), msg),
  d: (tag, msg) => __android_log_write(ANDROID_LOG_DEBUG, "Gecko" + tag.substring(0, MAX_TAG_LENGTH), msg),
  i: (tag, msg) => __android_log_write(ANDROID_LOG_INFO, "Gecko" + tag.substring(0, MAX_TAG_LENGTH), msg),
  w: (tag, msg) => __android_log_write(ANDROID_LOG_WARN, "Gecko" + tag.substring(0, MAX_TAG_LENGTH), msg),
  e: (tag, msg) => __android_log_write(ANDROID_LOG_ERROR, "Gecko" + tag.substring(0, MAX_TAG_LENGTH), msg),

  bind: function(tag) {
    return {
      MAX_TAG_LENGTH: MAX_TAG_LENGTH,
      v: AndroidLog.v.bind(null, tag),
      d: AndroidLog.d.bind(null, tag),
      i: AndroidLog.i.bind(null, tag),
      w: AndroidLog.w.bind(null, tag),
      e: AndroidLog.e.bind(null, tag),
    };
  },
};

if (typeof Components == "undefined") {
  // Specify exported symbols for require.js module loader.
  // eslint-disable-next-line no-undef
  module.exports = AndroidLog;
}
