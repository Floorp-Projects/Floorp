/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

this.EXPORTED_SYMBOLS = ["DeferredSave"];

// If delay parameter is not provided, default is 50 milliseconds.
const DEFAULT_SAVE_DELAY_MS = 50;

/**
 * A module to manage deferred, asynchronous writing of data files
 * to disk. Writing is deferred by waiting for a specified delay after
 * a request to save the data, before beginning to write. If more than
 * one save request is received during the delay, all requests are
 * fulfilled by a single write.
 *
 * @constructor
 * @param aPath
 *        String representing the full path of the file where the data
 *        is to be written.
 * @param aDataProvider
 *        Callback function that takes no argument and returns the data to
 *        be written. If aDataProvider returns an ArrayBufferView, the
 *        bytes it contains are written to the file as is.
 *        If aDataProvider returns a String the data are UTF-8 encoded
 *        and then written to the file.
 * @param [optional] aDelay
 *        The delay in milliseconds between the first saveChanges() call
 *        that marks the data as needing to be saved, and when the DeferredSave
 *        begins writing the data to disk. Default 50 milliseconds.
 */
this.DeferredSave = function (aPath, aDataProvider, aDelay) {
  // Set up loggers for this instance of DeferredSave
  let leafName = OS.Path.basename(aPath);
  Cu.import("resource://gre/modules/AddonLogging.jsm");
  LogManager.getLogger("DeferredSave/" + leafName, this);

  // @type {Deferred|null}, null when no data needs to be written
  // @resolves with the result of OS.File.writeAtomic when all writes complete
  // @rejects with the error from OS.File.writeAtomic if the write fails,
  //          or with the error from aDataProvider() if that throws.
  this._pending = null;

  // @type {Promise}, completes when the in-progress write (if any) completes,
  //       kept as a resolved promise at other times to simplify logic.
  //       Because _deferredSave() always uses _writing.then() to execute
  //       its next action, we don't need a special case for whether a write
  //       is in progress - if the previous write is complete (and the _writing
  //       promise is already resolved/rejected), _writing.then() starts
  //       the next action immediately.
  //
  // @resolves with the result of OS.File.writeAtomic
  // @rejects with the error from OS.File.writeAtomic
  this._writing = Promise.resolve(0);

  // Are we currently waiting for a write to complete
  this.writeInProgress = false;

  this._path = aPath;
  this._dataProvider = aDataProvider;

  this._timer = null;

  // Some counters for telemetry
  // The total number of times the file was written
  this.totalSaves = 0;

  // The number of times the data became dirty while
  // another save was in progress
  this.overlappedSaves = 0;

  // Error returned by the most recent write (if any)
  this._lastError = null;

  if (aDelay && (aDelay > 0))
    this._delay = aDelay;
  else
    this._delay = DEFAULT_SAVE_DELAY_MS;
}

this.DeferredSave.prototype = {
  get dirty() {
    return this._pending || this.writeInProgress;
  },

  get lastError() {
    return this._lastError;
  },

  // Start the pending timer if data is dirty
  _startTimer: function() {
    if (!this._pending) {
      return;
    }

    this.LOG("Starting timer");
    if (!this._timer)
      this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._timer.initWithCallback(() => this._deferredSave(),
                                 this._delay, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Mark the current stored data dirty, and schedule a flush to disk
   * @return A Promise<integer> that will be resolved after the data is written to disk;
   *         the promise is resolved with the number of bytes written.
   */
  saveChanges: function() {
    this.LOG("Save changes");
    if (!this._pending) {
      if (this.writeInProgress) {
        this.LOG("Data changed while write in progress");
        this.overlappedSaves++;
      }
      this._pending = Promise.defer();
      // Wait until the most recent write completes or fails (if it hasn't already)
      // and then restart our timer
      this._writing.then(count => this._startTimer(), error => this._startTimer());
    }
    return this._pending.promise;
  },

  _deferredSave: function() {
    let pending = this._pending;
    this._pending = null;
    let writing = this._writing;
    this._writing = pending.promise;

    // In either the success or the exception handling case, we don't need to handle
    // the error from _writing here; it's already being handled in another then()
    let toSave = null;
    try {
      toSave = this._dataProvider();
    }
    catch(e) {
      this.ERROR("Deferred save dataProvider failed", e);
      writing.then(null, error => {})
        .then(count => {
          pending.reject(e);
        });
      return;
    }

    writing.then(null, error => {return 0;})
    .then(count => {
      this.LOG("Starting write");
      this.totalSaves++;
      this.writeInProgress = true;

      OS.File.writeAtomic(this._path, toSave, {tmpPath: this._path + ".tmp"})
      .then(
        result => {
          this._lastError = null;
          this.writeInProgress = false;
          this.LOG("Write succeeded");
          pending.resolve(result);
        },
        error => {
          this._lastError = error;
          this.writeInProgress = false;
          this.WARN("Write failed", error);
          pending.reject(error);
        });
    });
  },

  /**
   * Immediately save the dirty data to disk, skipping
   * the delay of normal operation. Note that the write
   * still happens asynchronously in the worker
   * thread from OS.File.
   *
   * There are four possible situations:
   * 1) Nothing to flush
   * 2) Data is not currently being written, in-memory copy is dirty
   * 3) Data is currently being written, in-memory copy is clean
   * 4) Data is being written and in-memory copy is dirty
   *
   * @return Promise<integer> that will resolve when all in-memory data
   *         has finished being flushed, returning the number of bytes
   *         written. If all in-memory data is clean, completes with the
   *         result of the most recent write.
   */
  flush: function() {
    // If we have pending changes, cancel our timer and set up the write
    // immediately (_deferredSave queues the write for after the most
    // recent write completes, if it hasn't already)
    if (this._pending) {
      this.LOG("Flush called while data is dirty");
      if (this._timer) {
        this._timer.cancel();
        this._timer = null;
      }
      this._deferredSave();
    }

    return this._writing;
  }
}
