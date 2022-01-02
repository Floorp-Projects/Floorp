/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 489872 to make sure passing nulls to nsNavHistory doesn't crash.
 */

// Make an array of services to test, each specifying a class id, interface
// and an array of function names that don't throw when passed nulls
var testServices = [
  [
    "browser/nav-history-service;1",
    ["nsINavHistoryService"],
    [
      "queryStringToQuery",
      "removePagesByTimeframe",
      "removePagesFromHost",
      "getObservers",
    ],
  ],
  [
    "browser/nav-bookmarks-service;1",
    ["nsINavBookmarksService"],
    ["createFolder", "getObservers"],
  ],
  ["browser/favicon-service;1", ["nsIFaviconService"], []],
  ["browser/tagging-service;1", ["nsITaggingService"], []],
];
info(testServices.join("\n"));

function run_test() {
  for (let [cid, ifaces, nothrow] of testServices) {
    info(`Running test with ${cid} ${ifaces.join(", ")} ${nothrow}`);
    let s = Cc["@mozilla.org/" + cid].getService(Ci.nsISupports);
    for (let iface of ifaces) {
      s.QueryInterface(Ci[iface]);
    }

    let okName = function(name) {
      info(`Checking if function is okay to test: ${name}`);
      let func = s[name];

      let mesg = "";
      if (typeof func != "function") {
        mesg = "Not a function!";
      } else if (!func.length) {
        mesg = "No args needed!";
      } else if (name == "QueryInterface") {
        mesg = "Ignore QI!";
      }

      if (mesg) {
        info(`${mesg} Skipping: ${name}`);
        return false;
      }

      return true;
    };

    info(`Generating an array of functions to test service: ${s}`);
    for (let n of Object.keys(s)
      .filter(i => okName(i))
      .sort()) {
      info(`\nTesting ${ifaces.join(", ")} function with null args: ${n}`);

      let func = s[n];
      let num = func.length;
      info(`Generating array of nulls for #args: ${num}`);
      let args = Array(num).fill(null);

      let tryAgain = true;
      while (tryAgain) {
        try {
          info(`Calling with args: ${JSON.stringify(args)}`);
          func.apply(s, args);

          info(
            `The function did not throw! Is it one of the nothrow? ${nothrow}`
          );
          Assert.notEqual(nothrow.indexOf(n), -1);

          info("Must have been an expected nothrow, so no need to try again");
          tryAgain = false;
        } catch (ex) {
          if (ex.result == Cr.NS_ERROR_ILLEGAL_VALUE) {
            info(`Caught an expected exception: ${ex.name}`);
            info("Moving on to the next test..");
            tryAgain = false;
          } else if (ex.result == Cr.NS_ERROR_XPC_NEED_OUT_OBJECT) {
            let pos = Number(ex.message.match(/object arg (\d+)/)[1]);
            info(`Function call expects an out object at ${pos}`);
            args[pos] = {};
          } else if (ex.result == Cr.NS_ERROR_NOT_IMPLEMENTED) {
            info(`Method not implemented exception: ${ex.name}`);
            info("Moving on to the next test..");
            tryAgain = false;
          } else {
            throw ex;
          }
        }
      }
    }
  }
}
