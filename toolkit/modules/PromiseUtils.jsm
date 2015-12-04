/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

this.EXPORTED_SYMBOLS = ["PromiseUtils"];

Components.utils.import("resource://gre/modules/Timer.jsm");

this.PromiseUtils = {
  /*
   * Creates a new pending Promise and provide methods to resolve and reject this Promise.
   *
   * @return {Deferred} an object consisting of a pending Promise "promise"
   * and methods "resolve" and "reject" to change its state.
   */
  defer : function() {
    return new Deferred();
  },
}

/**
 * The definition of Deferred object which is returned by PromiseUtils.defer(),
 * It contains a Promise and methods to resolve/reject it.
 */
function Deferred() {
  /* A method to resolve the associated Promise with the value passed.
   * If the promise is already settled it does nothing.
   *
   * @param {anything} value : This value is used to resolve the promise
   * If the value is a Promise then the associated promise assumes the state
   * of Promise passed as value.
   */
  this.resolve = null;

  /* A method to reject the assocaited Promise with the value passed.
   * If the promise is already settled it does nothing.
   *
   * @param {anything} reason: The reason for the rejection of the Promise.
   * Generally its an Error object. If however a Promise is passed, then the Promise
   * itself will be the reason for rejection no matter the state of the Promise.
   */
  this.reject = null;

  /* A newly created Pomise object.
   * Initially in pending state.
   */
  this.promise = new Promise((resolve, reject) => {
    this.resolve = resolve;
    this.reject = reject;
  });
}
