/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var Cu = require('chrome').Cu;
var Cc = require('chrome').Cc;
var Ci = require('chrome').Ci;

/*
 * Minimalist implementation of ES6 promises built on SDK promises. 2 things to
 * know:
 * - There is a hack in .then() to be async which matches A+ and toolkit
 *   promises. GCLI code works with either sync or async promises, but async
 *   is more correct
 * - There is an additional Promise.defer function which to matches the call
 *   in toolkit promises, which in turn matches the call in SDK promises
 *
 * Why not use toolkit promises directly? Because there is a strange bug that
 * we are investigating where thread executions vanish.
 * 
 * When we've solved the debugger/sdk/promise/gcli/helpers/overlap problem then
 * we should use this instead:
 * module.exports = exports = require('resource://gre/modules/Promise.jsm');
 */

var promise = require('resource://gre/modules/devtools/deprecated-sync-thenables.js', {});

/**
 * An implementation of ES6 promises in terms of SDK promises
 */
var Promise = function(executor) {
  this.deferred = promise.defer();
  try {
    executor.call(null, this.deferred.resolve, this.deferred.reject);
  }
  catch (ex) {
    this.deferred.reject(ex);
  }
}

var async = true;

/**
 * The sync version of this would look like
 *     Promise.prototype.then = function(onResolve, onReject) {
 *      return this.deferred.promise.then(onResolve, onReject);
 *    };
 */
Promise.prototype.then = function(onResolve, onReject) {
  return new Promise(function(resolve, reject) {
    setTimeout(function() {
      try {
        resolve(this.deferred.promise.then(onResolve, onReject));
      }
      catch (ex) {
        reject(ex);
      }
    }.bind(this), 0);
  }.bind(this));
};

Promise.all = promise.all;
Promise.resolve = promise.resolve;
Promise.defer = promise.defer;

exports.Promise = Promise;


/**
 * Implementation of the setTimeout/clearTimeout web APIs taken from the old
 * Timer.jsm module
 */

// This gives us >=2^30 unique timer IDs, enough for 1 per ms for 12.4 days.
var nextTimeoutId = 1; // setTimeout must return a positive integer

var timeoutTable = new Map(); // int -> nsITimer

var setTimeout = function(callback, millis) {
  let id = nextTimeoutId++;
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(function setTimeout_timer() {
    timeoutTable.delete(id);
    callback.call(undefined);
  }, millis, timer.TYPE_ONE_SHOT);

  timeoutTable.set(id, timer);
  return id;
}

var clearTimeout = function(aId) {
  if (timeoutTable.has(aId)) {
    timeoutTable.get(aId).cancel();
    timeoutTable.delete(aId);
  }
}
