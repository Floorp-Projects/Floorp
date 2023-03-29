/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Log as StdLog } from "resource://gre/modules/Log.sys.mjs";

const PREF_REMOTE_LOG_LEVEL = "remote.log.level";

/** E10s compatible wrapper for the standard logger from Log.sys.mjs. */
export class Log {
  static TYPES = {
    CDP: "CDP",
    MARIONETTE: "Marionette",
    REMOTE_AGENT: "RemoteAgent",
    WEBDRIVER_BIDI: "WebDriver BiDi",
  };

  /**
   * Get a logger instance. For each provided type, a dedicated logger instance
   * will be returned, but all loggers are relying on the same preference.
   *
   * @param {String} type
   *     The type of logger to use. Protocol-specific modules should use the
   *     corresponding logger type. Eg. files under /marionette should use
   *     Log.TYPES.MARIONETTE.
   */
  static get(type = Log.TYPES.REMOTE_AGENT) {
    const logger = StdLog.repository.getLogger(type);
    if (!logger.ownAppenders.length) {
      logger.addAppender(new StdLog.DumpAppender());
      logger.manageLevelFromPref(PREF_REMOTE_LOG_LEVEL);
    }
    return logger;
  }

  /**
   * Check if the current log level matches the Trace log level. This should be
   * used to guard logger.trace calls and avoid instanciating logger instances
   * unnecessarily.
   */
  static get isTraceLevel() {
    return [StdLog.Level.All, StdLog.Level.Trace].includes(
      PREF_REMOTE_LOG_LEVEL
    );
  }

  static get verbose() {
    // we can't use Preferences.sys.mjs before first paint,
    // see ../browser/base/content/test/performance/browser_startup.js
    const level = Services.prefs.getStringPref(PREF_REMOTE_LOG_LEVEL, "Info");
    return StdLog.Level[level] >= StdLog.Level.Info;
  }
}
