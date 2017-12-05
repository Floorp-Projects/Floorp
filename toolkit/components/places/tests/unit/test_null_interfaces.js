/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 489872 to make sure passing nulls to nsNavHistory doesn't crash.
 */

// Make an array of services to test, each specifying a class id, interface
// and an array of function names that don't throw when passed nulls
var testServices = [
  ["browser/nav-history-service;1",
    ["nsINavHistoryService"],
    ["queryStringToQueries", "removePagesByTimeframe", "removePagesFromHost", "getObservers"]
  ],
  ["browser/nav-bookmarks-service;1",
    ["nsINavBookmarksService", "nsINavHistoryObserver"],
    ["createFolder", "getObservers", "onFrecencyChanged", "onTitleChanged", "onDeleteURI"]
  ],
  ["browser/livemark-service;2", ["mozIAsyncLivemarks"], ["reloadLivemarks"]],
  ["browser/annotation-service;1", ["nsIAnnotationService"], ["getObservers"]],
  ["browser/favicon-service;1", ["nsIFaviconService"], []],
  ["browser/tagging-service;1", ["nsITaggingService"], []],
];
do_print(testServices.join("\n"));

function run_test() {
  for (let [cid, ifaces, nothrow] of testServices) {
    do_print(`Running test with ${cid} ${ifaces.join(", ")} ${nothrow}`);
    let s = Cc["@mozilla.org/" + cid].getService(Ci.nsISupports);
    for (let iface of ifaces) {
      s.QueryInterface(Ci[iface]);
    }

    let okName = function(name) {
      do_print(`Checking if function is okay to test: ${name}`);
      let func = s[name];

      let mesg = "";
      if (typeof func != "function")
        mesg = "Not a function!";
      else if (func.length == 0)
        mesg = "No args needed!";
      else if (name == "QueryInterface")
        mesg = "Ignore QI!";

      if (mesg) {
        do_print(`${mesg} Skipping: ${name}`);
        return false;
      }

      return true;
    };

    do_print(`Generating an array of functions to test service: ${s}`);
    for (let n of Object.keys(s).filter(i => okName(i)).sort()) {
      do_print(`\nTesting ${ifaces.join(", ")} function with null args: ${n}`);

      let func = s[n];
      let num = func.length;
      do_print(`Generating array of nulls for #args: ${num}`);
      let args = Array(num).fill(null);

      let tryAgain = true;
      while (tryAgain == true) {
        try {
          do_print(`Calling with args: ${JSON.stringify(args)}`);
          func.apply(s, args);

          do_print(`The function did not throw! Is it one of the nothrow? ${nothrow}`);
          Assert.notEqual(nothrow.indexOf(n), -1);

          do_print("Must have been an expected nothrow, so no need to try again");
          tryAgain = false;
        } catch (ex) {
          if (ex.result == Cr.NS_ERROR_ILLEGAL_VALUE) {
            do_print(`Caught an expected exception: ${ex.name}`);
            do_print("Moving on to the next test..");
            tryAgain = false;
          } else if (ex.result == Cr.NS_ERROR_XPC_NEED_OUT_OBJECT) {
            let pos = Number(ex.message.match(/object arg (\d+)/)[1]);
            do_print(`Function call expects an out object at ${pos}`);
            args[pos] = {};
          } else if (ex.result == Cr.NS_ERROR_NOT_IMPLEMENTED) {
            do_print(`Method not implemented exception: ${ex.name}`);
            do_print("Moving on to the next test..");
            tryAgain = false;
          } else {
            throw ex;
          }
        }
      }
    }
  }
}
