/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Log"];

const {Log: StdLog} = ChromeUtils.import("resource://gre/modules/Log.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
    // we can't use Preferences.jsm before first paint,
    // see ../browser/base/content/test/performance/browser_startup.js
    const level = Services.prefs.getStringPref(LOG_LEVEL, "Info");
    return StdLog.Level[level] >= StdLog.Level.Info;
  }
}
