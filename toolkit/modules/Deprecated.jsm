/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Deprecated"];

const PREF_DEPRECATION_WARNINGS = "devtools.errorconsole.deprecation_warnings";

// A flag that indicates whether deprecation warnings should be logged.
var logWarnings = Services.prefs.getBoolPref(PREF_DEPRECATION_WARNINGS);

Services.prefs.addObserver(PREF_DEPRECATION_WARNINGS, function(
  aSubject,
  aTopic,
  aData
) {
  logWarnings = Services.prefs.getBoolPref(PREF_DEPRECATION_WARNINGS);
});

/**
 * Build a callstack log message.
 *
 * @param nsIStackFrame aStack
 *        A callstack to be converted into a string log message.
 */
function stringifyCallstack(aStack) {
  // If aStack is invalid, use Components.stack (ignoring the last frame).
  if (!aStack || !(aStack instanceof Ci.nsIStackFrame)) {
    aStack = Components.stack.caller;
  }

  let frame = aStack.caller;
  let msg = "";
  // Get every frame in the callstack.
  while (frame) {
    if (frame.filename || frame.lineNumber || frame.name) {
      msg += frame.filename + " " + frame.lineNumber + " " + frame.name + "\n";
    }
    frame = frame.caller;
  }
  return msg;
}

var Deprecated = {
  /**
   * Log a deprecation warning.
   *
   * @param string aText
   *        Deprecation warning text.
   * @param string aUrl
   *        A URL pointing to documentation describing deprecation
   *        and the way to address it.
   * @param nsIStackFrame aStack
   *        An optional callstack. If it is not provided a
   *        snapshot of the current JavaScript callstack will be
   *        logged.
   */
  warning(aText, aUrl, aStack) {
    if (!logWarnings) {
      return;
    }

    // If URL is not provided, report an error.
    if (!aUrl) {
      Cu.reportError(
        "Error in Deprecated.warning: warnings must " +
          "provide a URL documenting this deprecation."
      );
      return;
    }

    let textMessage =
      "DEPRECATION WARNING: " +
      aText +
      "\nYou may find more details about this deprecation at: " +
      aUrl +
      "\n" +
      // Append a callstack part to the deprecation message.
      stringifyCallstack(aStack);

    // Report deprecation warning.
    Cu.reportError(textMessage);
  },
};
