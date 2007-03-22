/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Sayre <sayrer@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

TestRunner.logEnabled = true;
TestRunner.logger = new Logger();

// Check the query string for arguments
var params = parseQueryString(location.search.substring(1), true);

// log levels for console and logfile
var fileLevel =  params.fileLevel || null;
var consoleLevel = params.consoleLevel || null;

// closeWhenDone tells us to call quit.js when complete
if (params.closeWhenDone) {
  TestRunner.onComplete = goQuitApplication;
}

// logFile to write our results
if (params.logFile) {
  MozillaFileLogger.init(params.logFile);
  TestRunner.logger.addListener("mozLogger", fileLevel + "", MozillaFileLogger.getLogCallback());
}

// if we get a quiet param, don't log to the console
if (!params.quiet) {
  function dumpListener(msg) {
    dump("*** " + msg.num + " " + msg.level + " " + msg.info.join(' ') + "\n");
  }
  TestRunner.logger.addListener("dumpListener", consoleLevel + "", dumpListener);
}

var gTestList = [];
var RunSet = {}
RunSet.runall = function(e) {
  TestRunner.runTests(gTestList);
}
RunSet.reloadAndRunAll = function(e) {
  e.preventDefault();
  //window.location.hash = "";
  var addParam = "";
  if (params.autorun) {
    window.location.search += "";
    window.location.href = window.location.href;
  } else if (window.location.search) {
    window.location.href += "&autorun=1";
  } else {
    window.location.href += "?autorun=1";
  }
  
};

// UI Stuff
function toggleVisible(elem) {
    toggleElementClass("invisible", elem);
}

function makeVisible(elem) {
    removeElementClass(elem, "invisible");
}

function makeInvisible(elem) {
    addElementClass(elem, "invisible");
}

function isVisible(elem) {
    // you may also want to check for
    // getElement(elem).style.display == "none"
    return !hasElementClass(elem, "invisible");
};

function toggleNonTests (e) {
  e.preventDefault();
  var elems = getElementsByTagAndClassName("*", "non-test");
  for (var i="0"; i<elems.length; i++) {
    toggleVisible(elems[i]);
  }
  if (isVisible(elems[0])) {
    $("toggleNonTests").innerHTML = "Hide Non-Tests";
  } else {
    $("toggleNonTests").innerHTML = "Show Non-Tests";
  }
}

// hook up our buttons
function hookup() {
  connect("runtests", "onclick", RunSet, "reloadAndRunAll");
  connect("toggleNonTests", "onclick", toggleNonTests);
  // run automatically if
  if (params.autorun) {
    RunSet.runall();
  }
}
