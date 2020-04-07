"use strict";

var ioService = Cc["@mozilla.org/network/io-service;1"].getService(
  Ci.nsIIOService
);

function run_test() {
  let testURIs = [
    ["http://example.com", false],
    ["about:robots", false],
    // 10/8 prefix (RFC 1918)
    ["http://9.255.255.255", false],
    ["http://10.0.0.0", true],
    ["http://10.0.23.31", true],
    ["http://10.255.255.255", true],
    ["http://11.0.0.0", false],
    // 169.254/16 prefix (Link Local)
    ["http://169.253.255.255", false],
    ["http://169.254.0.0", true],
    ["http://169.254.42.91", true],
    ["http://169.254.255.255", true],
    ["http://169.255.0.0", false],
    // 172.16/12 prefix (RFC 1918)
    ["http://172.15.255.255", false],
    ["http://172.16.0.0", true],
    ["http://172.25.110.0", true],
    ["http://172.31.255.255", true],
    ["http://172.32.0.0", false],
    // 192.168/16 prefix (RFC 1918)
    ["http://192.167.255.255", false],
    ["http://192.168.0.0", true],
    ["http://192.168.127.10", true],
    ["http://192.168.255.255", true],
    ["http://192.169.0.0", false],
  ];

  for (let [uri, isLocal] of testURIs) {
    let nsuri = ioService.newURI(uri);
    equal(isLocal, ioService.hostnameIsLocalIPAddress(nsuri));
  }
}
