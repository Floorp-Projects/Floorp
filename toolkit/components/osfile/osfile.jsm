/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Common front for various implementations of OS.File
 */

if (typeof Components != "undefined") {
  // eslint-disable-next-line mozilla/reject-global-this
  this.EXPORTED_SYMBOLS = ["OS"];
  const { OS } = ChromeUtils.import(
    "resource://gre/modules/osfile/osfile_async_front.jsm"
  );
  // eslint-disable-next-line mozilla/reject-global-this
  this.OS = OS;
} else {
  /* eslint-env worker */
  /* import-globals-from /toolkit/components/workerloader/require.js */
  importScripts("resource://gre/modules/workers/require.js");

  var SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");

  /* eslint-disable no-unused-vars */

  // At this stage, we need to import all sources at once to avoid
  // a unique failure on tbpl + talos that seems caused by a
  // what looks like a nested event loop bug (see bug 794091).
  if (SharedAll.Constants.Win) {
    importScripts(
      "resource://gre/modules/osfile/osfile_win_back.js",
      "resource://gre/modules/osfile/osfile_shared_front.js",
      "resource://gre/modules/osfile/osfile_win_front.js"
    );
  } else {
    importScripts(
      "resource://gre/modules/osfile/osfile_unix_back.js",
      "resource://gre/modules/osfile/osfile_shared_front.js",
      "resource://gre/modules/osfile/osfile_unix_front.js"
    );
  }

  /* eslint-enable no-unused-vars */

  OS.Path = require("resource://gre/modules/osfile/ospath.jsm");
}
