/* vim:set ts=2 sw=2 sts=2 ci et: */
/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file contains functions that are useful for debugging purposes from
// within JavaScript code.

this.EXPORTED_SYMBOLS = ["NS_ASSERT"];

var gTraceOnAssert = false;

/**
 * This function provides a simple assertion function for JavaScript.
 * If the condition is true, this function will do nothing.  If the
 * condition is false, then the message will be printed to the console
 * and an alert will appear showing a stack trace, so that the (alpha
 * or nightly) user can file a bug containing it.  For future enhancements,
 * see bugs 330077 and 330078.
 *
 * To suppress the dialogs, you can run with the environment variable
 * XUL_ASSERT_PROMPT set to 0 (if unset, this defaults to 1).
 *
 * @param condition represents the condition that we're asserting to be
 *                  true when we call this function--should be
 *                  something that can be evaluated as a boolean.
 * @param message   a string to be displayed upon failure of the assertion
 */

this.NS_ASSERT = function NS_ASSERT(condition, message) {
  if (condition)
    return;

  var releaseBuild = true;
  var defB = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefService)
                       .getDefaultBranch(null);
  try {
    switch (defB.getCharPref("app.update.channel")) {
      case "nightly":
      case "aurora":
      case "beta":
      case "default":
        releaseBuild = false;
    }
  } catch (ex) {}

  var assertionText = "ASSERT: " + message + "\n";

  // Report the error to the console
  Components.utils.reportError(assertionText);

  if (releaseBuild) {
    return;
  }

  // dump the stack to stdout too in non-release builds
  var stackText = "";
  if (gTraceOnAssert) {
    stackText = "Stack Trace: \n";
    var count = 0;
    // eslint-disable-next-line no-caller
    var caller = arguments.callee.caller;
    while (caller) {
      stackText += count++ + ":" + caller.name + "(";
      for (var i = 0; i < caller.arguments.length; ++i) {
        var arg = caller.arguments[i];
        stackText += arg;
        if (i < caller.arguments.length - 1)
          stackText += ",";
      }
      stackText += ")\n";
      caller = caller.arguments.callee.caller;
    }
  }

  dump(assertionText + stackText);
};
