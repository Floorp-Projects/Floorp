/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Trivial worker definition

importScripts("resource://gre/modules/workers/require.js");
var PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

var worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function(method, args = []) {
  return Agent[method](...args);
},
worker.postMessage = function(...args) {
  self.postMessage(...args);
};
worker.close = function() {
  self.close();
};
worker.log = function(...args) {
  dump("Worker: " + args.join(" ") + "\n");
};
self.addEventListener("message", msg => worker.handleMessage(msg));

var Agent = {
  bounce: function(...args) {
    return args;
  }
};
