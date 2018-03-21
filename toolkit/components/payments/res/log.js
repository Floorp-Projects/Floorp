/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file defines a fallback log object to be used during development outside
 * of the paymentDialogWrapper. When loaded in the wrapper, a frame script
 * providing pref-controlled logging overwrites these methods.
 */

/* eslint-disable no-console */
/* exported log */

"use strict";

var log = {
  error(...args) {
    console.error("log.js", ...args);
  },
  warn(...args) {
    console.warn("log.js", ...args);
  },
  info(...args) {
    console.info("log.js", ...args);
  },
  debug(...args) {
    console.debug("log.js", ...args);
  },
};
