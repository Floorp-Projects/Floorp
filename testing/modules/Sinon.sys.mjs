/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ================================================
// Load mocking/stubbing library sinon
// docs: http://sinonjs.org/releases/v7.2.7/
import {
  clearInterval,
  clearTimeout,
  setInterval,
  setIntervalWithTarget,
  setTimeout,
  setTimeoutWithTarget,
} from "resource://gre/modules/Timer.sys.mjs";

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
  clearInterval,
  clearTimeout,
  setInterval,
  setIntervalWithTarget,
  setTimeout,
  setTimeoutWithTarget,
};
Services.scriptloader.loadSubScript(
  "resource://testing-common/sinon-7.2.7.js",
  obj
);

export const sinon = obj.global.sinon;
// ================================================
