/* check resource: protocol for traversal problems */

"use strict";

const specs = [
  "resource:///chrome/../plugins",
  "resource:///chrome%2f../plugins",
  "resource:///chrome/..%2fplugins",
  "resource:///chrome%2f%2e%2e%2fplugins",
  "resource:///../../../..",
  "resource:///..%2f..%2f..%2f..",
  "resource:///%2e%2e",
];

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

  for (var spec of specs) {
    var uri = ios.newURI(spec);
    if (uri.spec.includes("..")) {
      do_throw(
        "resource: traversal remains: '" + spec + "' ==> '" + uri.spec + "'"
      );
    }
  }
}
