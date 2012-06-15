/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Common front for various implementations of OS.File
 */

if (typeof Components != "undefined") {
  // We do not wish osfile.jsm to be used directly as a main thread
  // module yet.
  throw new Error("osfile.jsm cannot be used from the main thread yet");
}
#ifdef XP_WIN
importScripts("resource://gre/modules/osfile/osfile_win_front.jsm");
#else
importScripts("resource://gre/modules/osfile/osfile_unix_front.jsm");
#endif
