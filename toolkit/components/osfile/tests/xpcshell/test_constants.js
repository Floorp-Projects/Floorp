/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/Promise.jsm", this);
Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm", this);
let isDebugBuild = Components.classes["@mozilla.org/xpcom/debug;1"].getService(Components.interfaces.nsIDebug2).isDebugBuild;
// import the worker using a chrome uri
let worker_url = "chrome://workers/content/worker_test_constants.js";
do_load_manifest("data/chrome.manifest");

function run_test() {
  run_next_test();
}

// Test that OS.Constants is defined correctly.
add_task(function* check_definition() {
  do_check_true(OS.Constants!=null);
  do_check_true(!!OS.Constants.Win || !!OS.Constants.libc);
  do_check_true(OS.Constants.Path!=null);
  do_check_true(OS.Constants.Sys!=null);
  // check system name
  do_check_eq(OS.Constants.Sys.Name, Services.appinfo.OS);
  // check if using DEBUG build
  if (isDebugBuild) {
    do_check_true(OS.Constants.Sys.DEBUG);
  } else {
    do_check_true(typeof(OS.Constants.Sys.DEBUG) == 'undefined');
  }
});

function test_worker(worker) {
  var deferred = Promise.defer();
  worker.onmessage = function (e) {
    // parse the stringified data
    var data = JSON.parse(e.data);
    try {
      // perform the tests
      do_check_true(data.OS_Constants != null);
      do_check_true(!!data.OS_Constants_Win || !!data.OS_Constants_libc);
      do_check_true(data.OS_Constants_Path != null);
      do_check_true(data.OS_Constants_Sys != null);
      do_check_eq(data.OS_Constants_Sys_Name, Services.appinfo.OS);
      if (isDebugBuild) {
        do_check_true(data.OS_Constants_Sys_DEBUG);
      } else {
        do_check_true(typeof(data.OS_Constants_Sys_DEBUG) == 'undefined');
      }
    }  finally {
       // resolve the promise
       deferred.resolve();
    }
  };
  // start the worker
  worker.postMessage('Start tests');
  return deferred.promise;
}

add_task(function* check_worker() {
  // create a new ChromeWorker
  return test_worker(new ChromeWorker(worker_url));
});