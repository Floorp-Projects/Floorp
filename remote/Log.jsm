/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Log"];

/** E10s compatible wrapper for the standard logger from Log.jsm. */
class Log {
  static get() {
    const StdLog = ChromeUtils.import("resource://gre/modules/Log.jsm").Log;
    const logger = StdLog.repository.getLogger("RemoteAgent");
    if (logger.ownAppenders.length == 0) {
      logger.addAppender(new StdLog.DumpAppender());
      logger.manageLevelFromPref("remote.log.level");
    }
    return logger;
  }
};
