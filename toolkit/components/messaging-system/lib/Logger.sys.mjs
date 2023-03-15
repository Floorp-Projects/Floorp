/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ConsoleAPI } from "resource://gre/modules/Console.sys.mjs";

const LOGGING_PREF = "messaging-system.log";

export class Logger extends ConsoleAPI {
  constructor(name) {
    let consoleOptions = {
      prefix: name,
      maxLogLevel: Services.prefs.getCharPref(LOGGING_PREF, "warn"),
      maxLogLevelPref: LOGGING_PREF,
    };
    super(consoleOptions);
  }
}
