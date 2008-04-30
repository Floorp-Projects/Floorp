# vim:set ts=2 sw=2 sts=2 ci et:
# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Places Bookmark Properties.
#
# The Initial Developer of the Original Code is
# Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Joe Hughes <jhughes@google.com>
#   Ben Goodger <beng@google.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
#
# This file contains functions that are useful for debugging purposes from
# within JavaScript code.

var EXPORTED_SYMBOLS = ["NS_ASSERT"];

var gTraceOnAssert = true;

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

function NS_ASSERT(condition, message) {
  if (condition)
    return;

  var releaseBuild = true;
  var defB = Components.classes["@mozilla.org/preferences-service;1"]
                       .getService(Components.interfaces.nsIPrefService)
                       .getDefaultBranch(null);
  try {
    switch (defB.getCharPref("app.update.channel")) {
      case "nightly":
      case "beta":
      case "default":
        releaseBuild = false;
    }
  } catch(ex) {}

  var caller = arguments.callee.caller;
  var assertionText = "ASSERT: " + message + "\n";

  if (releaseBuild) {
    // Just report the error to the console
    Components.utils.reportError(assertionText);
    return;
  }

  // Otherwise, dump to stdout and launch an assertion failure dialog
  dump(assertionText);

  var stackText = "";
  if (gTraceOnAssert) {
    stackText = "Stack Trace: \n";
    var count = 0;
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

  var environment = Components.classes["@mozilla.org/process/environment;1"].
                    getService(Components.interfaces.nsIEnvironment);
  if (environment.exists("XUL_ASSERT_PROMPT") &&
      !parseInt(environment.get("XUL_ASSERT_PROMPT")))
    return;

  var source = null;
  if (this.window)
    source = this.window;
  var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
           getService(Components.interfaces.nsIPromptService);
  ps.alert(source, "Assertion Failed", assertionText + stackText);
}
