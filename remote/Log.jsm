/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Log"];

const {Preferences} = ChromeUtils.import("resource://gre/modules/Preferences.jsm");
const {Log: StdLog} = ChromeUtils.import("resource://gre/modules/Log.jsm");

const LOG_LEVEL = "remote.log.level";

/** E10s compatible wrapper for the standard logger from Log.jsm. */
class Log {
  static get() {
    const logger = StdLog.repository.getLogger("RemoteAgent");
    if (logger.ownAppenders.length == 0) {
      logger.addAppender(new StdLog.DumpAppender());
      logger.manageLevelFromPref(LOG_LEVEL);
    }
    return logger;
  }

  static get verbose() {
    return StdLog.Level[Preferences.get(LOG_LEVEL)] >= StdLog.Level.Info;
  }
}
