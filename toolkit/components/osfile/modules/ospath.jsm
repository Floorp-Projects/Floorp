/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handling native paths.
 *
 * This module contains a number of functions destined to simplify
 * working with native paths through a cross-platform API. Functions
 * of this module will only work with the following assumptions:
 *
 * - paths are valid;
 * - paths are defined with one of the grammars that this module can
 *   parse (see later);
 * - all path concatenations go through function |join|.
 */

/* global OS */
/* eslint-env node */

"use strict";

if (typeof Components == "undefined") {
  let Path;
  if (OS.Constants.Win) {
    Path = require("resource://gre/modules/osfile/ospath_win.jsm");
  } else {
    Path = require("resource://gre/modules/osfile/ospath_unix.jsm");
  }
  module.exports = Path;
} else {
  let Scope = {};
  ChromeUtils.import(
    "resource://gre/modules/osfile/osfile_shared_allthreads.jsm",
    Scope
  );

  let Path = {};
  if (Scope.OS.Constants.Win) {
    ChromeUtils.import("resource://gre/modules/osfile/ospath_win.jsm", Path);
  } else {
    ChromeUtils.import("resource://gre/modules/osfile/ospath_unix.jsm", Path);
  }

  this.EXPORTED_SYMBOLS = [];
  for (let k in Path) {
    EXPORTED_SYMBOLS.push(k);
    this[k] = Path[k];
  }
}
