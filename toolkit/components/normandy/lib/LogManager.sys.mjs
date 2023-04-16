/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Log } from "resource://gre/modules/Log.sys.mjs";

const ROOT_LOGGER_NAME = "app.normandy";
let rootLogger = null;

export var LogManager = {
  /**
   * Configure the root logger for the Recipe Client. Must be called at
   * least once before using any loggers created via getLogger.
   * @param {Number} loggingLevel
   *        Logging level to use as defined in Log.sys.mjs
   */
  configure(loggingLevel) {
    if (!rootLogger) {
      rootLogger = Log.repository.getLogger(ROOT_LOGGER_NAME);
      rootLogger.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
    }
    rootLogger.level = loggingLevel;
  },

  /**
   * Obtain a named logger with the recipe client logger as its parent.
   * @param {String} name
   *        Name of the logger to obtain.
   * @return {Logger}
   */
  getLogger(name) {
    return Log.repository.getLogger(`${ROOT_LOGGER_NAME}.${name}`);
  },
};
