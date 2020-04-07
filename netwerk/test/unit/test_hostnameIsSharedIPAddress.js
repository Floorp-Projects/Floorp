"use strict";

var ioService = Cc["@mozilla.org/network/io-service;1"].getService(
  Ci.nsIIOService
);

function run_test() {
  let testURIs = [
    // 100.64/10 prefix (RFC 6598)
    ["http://100.63.255.254", false],
    ["http://100.64.0.0", true],
    ["http://100.91.63.42", true],
    ["http://100.127.255.254", true],
    ["http://100.128.0.0", false],
  ];

  for (let [uri, isShared] of testURIs) {
    let nsuri = ioService.newURI(uri);
    equal(isShared, ioService.hostnameIsSharedIPAddress(nsuri));
  }
}
