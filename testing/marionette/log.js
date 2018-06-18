/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
const StdLog = ChromeUtils.import("resource://gre/modules/Log.jsm", {}).Log;

const {MarionettePrefs} = ChromeUtils.import("chrome://marionette/content/prefs.js", {});

this.EXPORTED_SYMBOLS = ["Log"];

const isChildProcess = Services.appinfo.processType ==
    Services.appinfo.PROCESS_TYPE_CONTENT;

/**
 * Shorthand for accessing the Marionette logging repository.
 *
 * Using this class to retrieve the `Log.jsm` repository for
 * Marionette will ensure the logger is set up correctly with the
 * appropriate stdout dumper and with the correct log level.
 *
 * Unlike `Log.jsm` this logger is E10s safe, meaning repository
 * configuration for appenders and logging levels are communicated
 * across processes.
 *
 * @name Log
 */
class MarionetteLog {
  /**
   * Obtain the `Marionette` logger.
   *
   * The returned {@link Logger} instance is shared among all
   * callers in the same process.
   *
   * @return {Logger}
   */
  static get() {
    let logger = StdLog.repository.getLogger("Marionette");
    if (logger.ownAppenders.length == 0) {
      logger.addAppender(new StdLog.DumpAppender());
    }
    logger.level = MarionettePrefs.logLevel;
    return logger;
  }

  /**
   * Obtain a logger that logs all messages with a prefix.
   *
   * Unlike {@link LoggerRepository.getLoggerWithMessagePrefix()}
   * this function will ensure invoke {@link #get()} first to ensure
   * the logger has been properly set up first.
   *
   * This returns a new object with a prototype chain that chains
   * up the original {@link Logger} instance.  The new prototype has
   * log functions that prefix `prefix` to each message.
   *
   * @param {string} prefix
   *     String to prefix each logged message with.
   *
   * @return {Proxy.<Logger>}
   */
  static getWithPrefix(prefix) {
    this.get();
    return StdLog.repository.getLoggerWithMessagePrefix("Marionette", `[${prefix}] `);
  }
}

class ParentProcessLog extends MarionetteLog {
  static get() {
    let logger = super.get();
    Services.ppmm.initialProcessData["Marionette:Log"] = {level: logger.level};
    return logger;
  }
}

class ChildProcessLog extends MarionetteLog {
  static get() {
    let logger = super.get();

    // Log.jsm is not e10s compatible (see https://bugzil.la/1411513)
    // so loading it in a new child process will reset the repository config
    logger.level = Services.cpmm.initialProcessData["Marionette:Log"] || StdLog.Level.Info;

    return logger;
  }
}

if (isChildProcess) {
  this.Log = ChildProcessLog;
} else {
  this.Log = ParentProcessLog;
}
