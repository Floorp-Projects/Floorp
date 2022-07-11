/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["sinon"];

// ================================================
// Load mocking/stubbing library sinon
// docs: http://sinonjs.org/releases/v7.2.7/
const {
  clearInterval,
  clearTimeout,
  setInterval,
  setIntervalWithTarget,
  setTimeout,
  setTimeoutWithTarget,
} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
// eslint-disable-next-line no-unused-vars
const obj = {
  global: {
    clearInterval,
    clearTimeout,
    setInterval,
    setIntervalWithTarget,
    setTimeout,
    setTimeoutWithTarget,
    Date,
  },
};
Services.scriptloader.loadSubScript(
  "resource://testing-common/sinon-7.2.7.js",
  obj
);
const sinon = obj.global.sinon;
// ================================================
