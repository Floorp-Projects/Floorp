"use strict";

const url = "http://foo.com/folder/file?/.";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

  var newURI = ios.newURI(url);
  Assert.equal(newURI.spec, url);
  Assert.equal(newURI.pathQueryRef, "/folder/file?/.");
  Assert.equal(newURI.resolve("./file?/."), url);
}
