/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  globals: {
    Cc: false,
    Ci: false,
    Cu: false,
    content: true,
    dumpLog: false,
    netscape: false,
    addMessageListener: false,
    goQuitApplication: false,
    MozillaFileLogger: false,
    Profiler: true,
    Services: false,
    gBrowser: false,
    removeMessageListener: false,
    sendAsyncMessage: false,
    sendSyncMessage: false,
    TalosPowersContent: true,
    TalosPowersParent: true,
    TalosContentProfiler: true,
    TalosParentProfiler: true,
    tpRecordTime: true,
  },

  plugins: ["mozilla"],

  rules: {
    "mozilla/avoid-Date-timing": "error",
  },
};
