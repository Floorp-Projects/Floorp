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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Boris Zbarsky <bzbarsky@mit.edu>
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

// This file contains common code that is loaded with each test file.

const nsIEventQueueService = Components.interfaces.nsIEventQueueService;
const nsIEventQueue        = Components.interfaces.nsIEventQueue;

var _eqs;
var _quit = false;
var _fail = false;
var _running_event_loop = false;
var _tests_pending = 0;

function _TimerCallback(expr) {
  this._expr = expr;
}
_TimerCallback.prototype = {
  _expr: "",
  QueryInterface: function(iid) {
    if (iid.Equals(Components.interfaces.nsITimerCallback) ||
        iid.Equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  notify: function(timer) {
    eval(this._expr);  
  }
};

function do_timeout(delay, expr) {
  var timer = Components.classes["@mozilla.org/timer;1"]
                        .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(new _TimerCallback(expr), delay, timer.TYPE_ONE_SHOT);
}

function do_main() {
  if (_quit)
    return;

  dump("*** running event loop\n");
  var eq = _eqs.getSpecialEventQueue(_eqs.CURRENT_THREAD_EVENT_QUEUE);

  _running_event_loop = true;
  eq.eventLoop();  // unblocked via interrupt from do_quit()
  _running_event_loop = false;

  // process any remaining events before exiting
  eq.processPendingEvents();
  eq.stopAcceptingEvents();
  eq.processPendingEvents();
}

function do_quit() {
  dump("*** exiting\n");

  _quit = true;

  if (_running_event_loop) {
    // interrupt the current thread to make eventLoop return.
    var thr = Components.classes["@mozilla.org/thread;1"]
                        .createInstance(Components.interfaces.nsIThread);
    thr.currentThread.interrupt();
  }
}

function do_throw(text) {
  _fail = true;
  do_quit();
  dump("*** CHECK FAILED: " + text + "\n");
  var frame = Components.stack;
  while (frame != null) {
    dump(frame + "\n");
    frame = frame.caller;
  }
  throw Components.results.NS_ERROR_ABORT;
}

function do_check_neq(_left, _right) {
  if (_left == _right)
    do_throw(_left + " != " + _right);
}

function do_check_eq(_left, _right) {
  if (_left != _right)
    do_throw(_left + " == " + _right);
}

function do_test_pending() {
  dump("*** test pending\n");
  _tests_pending++;
}

function do_test_finished() {
  dump("*** test finished\n");
  if (--_tests_pending == 0)
    do_quit();
}

// setup the main thread event queue
_eqs = Components.classes["@mozilla.org/event-queue-service;1"]
                 .getService(nsIEventQueueService);
_eqs.createMonitoredThreadEventQueue();
