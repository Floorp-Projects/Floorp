/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["logging"];

this.logging = {};

/** Simple logger that is used in Simple Test harness tests. */
logging.ContentLogger = class {
  constructor() {
    this.logs_ = [];
  }

  /**
   * Append a log entry.
   *
   * @param {string} message
   *     Log entry message.
   * @param {string=} level
   *     Severity of entry.  Defaults to "INFO".
   */
  log(message, level = "INFO") {
    let now = (new Date()).toString();
    this.logs_.push([level, message, now]);
  }

  /**
   * Append array of log entries.
   *
   * @param {Array.<Array<string, string, string>>} messages
   *     List of log entries, that are of the form severity, message,
   *     and date.
   */
  addAll(messages) {
    for (let message of messages) {
      this.logs_.push(message);
    }
  }

  /**
   * Gets current log entries and clears the cache.
   *
   * @return {Array.<Array<string, string, string>>}
   *     Log entries of the form severity, message, and date.
   */
  get() {
    let logs = this.logs_;
    this.logs_ = [];
    return logs;
  }
};

/**
 * Adapts an instance of ContentLogger for use in a sandbox.  Is consumed
 * by sandbox.augment.
 */
logging.Adapter = class {
  constructor(logger = null) {
    this.logger = logger;
  }

  get exports() {
    return new Map([["log", this.log.bind(this)]]);
  }

  log(message, level = "INFO") {
    dump(`MARIONETTE LOG: ${level}: ${message}\n`);
    if (this.logger) {
      this.logger.log(message, level);
    }
  }
};
