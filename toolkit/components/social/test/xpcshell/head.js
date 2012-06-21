/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
Cu.import("resource://gre/modules/Services.jsm");

const MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");

function AsyncRunner() {
  do_test_pending();
  do_register_cleanup((function () this.destroy()).bind(this));

  this._callbacks = {
    done: do_test_finished,
    error: function (err) {
      // xpcshell test functions like do_check_eq throw NS_ERROR_ABORT on
      // failure.  Ignore those so they aren't rethrown here.
      if (err !== Cr.NS_ERROR_ABORT) {
        if (err.stack) {
          err = err + " - See following stack:\n" + err.stack +
                      "\nUseless do_throw stack";
        }
        do_throw(err);
      }
    },
    consoleError: function (scriptErr) {
      // Try to ensure the error is related to the test.
      let filename = scriptErr.sourceName || scriptErr.toString() || "";
      if (filename.indexOf("/toolkit/components/social/") >= 0)
        do_throw(scriptErr);
    },
  };
  this._iteratorQueue = [];

  // This catches errors reported to the console, e.g., via Cu.reportError, but
  // not on the runner's stack.
  Cc["@mozilla.org/consoleservice;1"].
    getService(Ci.nsIConsoleService).
    registerListener(this);
}

AsyncRunner.prototype = {

  appendIterator: function appendIterator(iter) {
    this._iteratorQueue.push(iter);
  },

  next: function next(/* ... */) {
    if (!this._iteratorQueue.length) {
      this.destroy();
      this._callbacks.done();
      return;
    }

    // send() discards all arguments after the first, so there's no choice here
    // but to send only one argument to the yielder.
    let args = [arguments.length <= 1 ? arguments[0] : Array.slice(arguments)];
    try {
      var val = this._iteratorQueue[0].send.apply(this._iteratorQueue[0], args);
    }
    catch (err if err instanceof StopIteration) {
      this._iteratorQueue.shift();
      this.next();
      return;
    }
    catch (err) {
      this._callbacks.error(err);
    }

    // val is an iterator => prepend it to the queue and start on it
    // val is otherwise truthy => call next
    if (val) {
      if (typeof(val) != "boolean")
        this._iteratorQueue.unshift(val);
      this.next();
    }
  },

  destroy: function destroy() {
    Cc["@mozilla.org/consoleservice;1"].
      getService(Ci.nsIConsoleService).
      unregisterListener(this);
    this.destroy = function alreadyDestroyed() {};
  },

  observe: function observe(msg) {
    if (msg instanceof Ci.nsIScriptError)
      this._callbacks.consoleError(msg);
  },
};
