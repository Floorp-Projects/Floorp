/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["DeferredTask"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

/**
 * A task that will run after a delay. Multiple attempts to run the same task
 * before the delay will be coalesced
 *
 * Use this for instance if you write data to a file and you expect that you
 * may have to rewrite data very soon afterwards. With |DeferredTask|, the
 * task is delayed by a few milliseconds and, should a new change to the data
 * occur during this period,
 * 1/ only the final version of the data is actually written;
 * 2/ a further grace delay is added to take into account other changes.
 *
 * Constructor
 * @param aDelay The delay time in milliseconds.
 * @param aCallback The code to execute after the delay.
 */
this.DeferredTask = function DeferredTask(aCallback, aDelay) {
  this._callback = function onCallback() {
    this._timer = null;
    try {
      aCallback();
    } catch(e) {
      Cu.reportError(e);
    }
  }.bind(this);
  this._delay = aDelay;
}

DeferredTask.prototype = {
  /* Callback */
  _callback: null,
  /* Delay */
  _delay: null,
  /* Timer */
  _timer: null,

  /**
   * Check the current task state.
   * @returns true if pending, false otherwise.
   */
  get isPending() {
    return (this._timer != null);
  },

  /**
   * Start (or postpone) task.
   */
  start: function DeferredTask_start() {
    if (this._timer) {
      this._timer.cancel();
    }
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._timer.initWithCallback(
      this._callback, this._delay, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Perform any postponed task immediately.
   */
  flush: function DeferredTask_flush() {
    if (this._timer) {
      this.cancel();
      this._callback();
    }
  },

  /**
   * Cancel any pending task.
   */
  cancel: function DeferredTask_cancel() {
    if (this._timer) {
      this._timer.cancel();
      this._timer = null;
    }
  }
};
