/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = [
  "AsyncRunner",
];

const { interfaces: Ci, classes: Cc } = Components;

function AsyncRunner(callbacks) {
  this._callbacks = callbacks;
  this._iteratorQueue = [];

  // This catches errors reported to the console, e.g., via Cu.reportError.
  Cc["@mozilla.org/consoleservice;1"].
    getService(Ci.nsIConsoleService).
    registerListener(this);
}

AsyncRunner.prototype = {

  appendIterator: function AR_appendIterator(iter) {
    this._iteratorQueue.push(iter);
  },

  next: function AR_next(/* ... */) {
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

    // val is truthy => call next
    // val is an iterator => prepend it to the queue and start on it
    if (val) {
      if (typeof(val) != "boolean")
        this._iteratorQueue.unshift(val);
      this.next();
    }
  },

  destroy: function AR_destroy() {
    Cc["@mozilla.org/consoleservice;1"].
      getService(Ci.nsIConsoleService).
      unregisterListener(this);
    this.destroy = function AR_alreadyDestroyed() {};
  },

  observe: function AR_consoleServiceListener(msg) {
    if (msg instanceof Ci.nsIScriptError &&
        !(msg.flags & Ci.nsIScriptError.warningFlag))
    {
      this._callbacks.consoleError(msg);
    }
  },
};
