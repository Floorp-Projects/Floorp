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
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edilee@mozilla.com>
 *   Dan Mills <thunder@mozilla.com>
 *   Myk Melez <myk@mozilla.org>
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

let EXPORTED_SYMBOLS = ["Sync"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// Define some constants to specify various sync. callback states
const CB_READY = {};
const CB_COMPLETE = {};
const CB_FAIL = {};

// Share a secret only for functions in this file to prevent outside access
const SECRET = {};

/**
 * Check if the app is ready (not quitting)
 */
function checkAppReady() {
  // Watch for app-quit notification to stop any sync. calls
  let os = Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService);
  os.addObserver({
    observe: function observe() {
      // Now that the app is quitting, make checkAppReady throw
      checkAppReady = function() {
        throw Components.Exception("App. Quitting", Cr.NS_ERROR_ABORT);
      };
      os.removeObserver(this, "quit-application");
    }
  }, "quit-application", false);

  // In the common case, checkAppReady just returns true
  return (checkAppReady = function() true)();
};

/**
 * Create a callback that remembers state like whether it's been called
 */
function makeCallback() {
  // Initialize private callback data to prepare to be called
  let _ = {
    state: CB_READY,
    value: null
  };

  // The main callback remembers the value it's passed and that it got data
  let onComplete = function makeCallback_onComplete(data) {
    _.state = CB_COMPLETE;
    _.value = data;
  };

  // Only allow access to the private data if the secret matches
  onComplete._ = function onComplete__(secret) secret == SECRET ? _ : {};

  // Allow an alternate callback to trigger an exception to be thrown
  onComplete.throw = function onComplete_throw(data) {
    _.state = CB_FAIL;
    _.value = data;

    // Cause the caller to get an exception and stop execution
    throw data;
  };

  return onComplete;
}

/**
 * Make a synchronous version of the function object that will be called with
 * the provided thisArg.
 *
 * @param func {Function}
 *        The asynchronous function to make a synchronous function
 * @param thisArg {Object} [optional]
 *        The object that the function accesses with "this"
 * @param callback {Function} [optional] [internal]
 *        The callback that will trigger the end of the async. call
 * @usage let ret = Sync(asyncFunc, obj)(arg1, arg2);
 * @usage let ret = Sync(ignoreThisFunc)(arg1, arg2);
 * @usage let sync = Sync(async); let ret = sync(arg1, arg2);
 */
function Sync(func, thisArg, callback) {
  return function syncFunc(/* arg1, arg2, ... */) {
    // Grab the current thread so we can make it give up priority
    let thread = Cc["@mozilla.org/thread-manager;1"].getService().currentThread;

    // Save the original arguments into an array
    let args = Array.slice(arguments);

    let instanceCallback = callback;
    // We need to create a callback and insert it if we weren't given one
    if (instanceCallback == null) {
      // Create a new callback for this invocation instance and pass it in
      instanceCallback = makeCallback();
      args.unshift(instanceCallback);
    }

    // Call the async function bound to thisArg with the passed args
    func.apply(thisArg, args);

    // Keep waiting until our callback is triggered unless the app is quitting
    let callbackData = instanceCallback._(SECRET);
    while (checkAppReady() && callbackData.state == CB_READY)
      thread.processNextEvent(true);

    // Reset the state of the callback to prepare for another call
    let state = callbackData.state;
    callbackData.state = CB_READY;

    // Throw the value the callback decided to fail with
    if (state == CB_FAIL)
      throw callbackData.value;

    // Return the value passed to the callback
    return callbackData.value;
  };
}

/**
 * Make a synchronous version of an async. function and the callback to trigger
 * the end of the async. call.
 *
 * @param func {Function}
 *        The asynchronous function to make a synchronous function
 * @param thisArg {Object} [optional]
 *        The object that the function accesses with "this"
 * @usage let [sync, cb] = Sync.withCb(async); let ret = sync(arg1, arg2, cb);
 */
Sync.withCb = function Sync_withCb(func, thisArg) {
  let cb = makeCallback();
  return [Sync(func, thisArg, cb), cb];
};

/**
 * Set a timer, simulating the API for the window.setTimeout call.
 * This only simulates the API for the version of the call that accepts
 * a function as its first argument and no additional parameters,
 * and it doesn't return the timeout ID.
 *
 * @param func {Function}
 *        the function to call after the delay
 * @param delay {Number}
 *        the number of milliseconds to wait
 */
function setTimeout(func, delay) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  let callback = {
    notify: function notify() {
      // This line actually just keeps a reference to timer (prevent GC)
      timer = null;

      // Call the function so that "this" is global
      func();
    }
  }
  timer.initWithCallback(callback, delay, Ci.nsITimer.TYPE_ONE_SHOT);
}

function sleep(callback, milliseconds) {
  setTimeout(callback, milliseconds);
}

/**
 * Sleep the specified number of milliseconds, pausing execution of the caller
 * without halting the current thread.
 * For example, the following code pauses 1000ms between dumps:
 *
 *   dump("Wait for it...\n");
 *   Sync.sleep(1000);
 *   dump("Wait for it...\n");
 *   Sync.sleep(1000);
 *   dump("What are you waiting for?!\n");
 *
 * @param milliseconds {Number}
 *        The number of milliseconds to sleep
 */
Sync.sleep = Sync(sleep);
