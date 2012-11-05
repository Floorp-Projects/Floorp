/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "getTestLogger",
  "initTestLogging",
];

const {utils: Cu} = Components;

Cu.import("resource://services-common/log4moz.js");

this.initTestLogging = function initTestLogging(level) {
  function LogStats() {
    this.errorsLogged = 0;
  }
  LogStats.prototype = {
    format: function format(message) {
      if (message.level == Log4Moz.Level.Error) {
        this.errorsLogged += 1;
      }

      return message.loggerName + "\t" + message.levelDesc + "\t" +
        message.message + "\n";
    }
  };
  LogStats.prototype.__proto__ = new Log4Moz.Formatter();

  let log = Log4Moz.repository.rootLogger;
  let logStats = new LogStats();
  let appender = new Log4Moz.DumpAppender(logStats);

  if (typeof(level) == "undefined") {
    level = "Debug";
  }
  getTestLogger().level = Log4Moz.Level[level];
  Log4Moz.repository.getLogger("Services").level = Log4Moz.Level[level];

  log.level = Log4Moz.Level.Trace;
  appender.level = Log4Moz.Level.Trace;
  // Overwrite any other appenders (e.g. from previous incarnations)
  log.ownAppenders = [appender];
  log.updateAppenders();

  return logStats;
}

this.getTestLogger = function getTestLogger(component) {
  return Log4Moz.repository.getLogger("Testing");
}

