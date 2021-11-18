"use strict";

const url = "http://foo.com/folder/file?/.";

function run_test() {
  var newURI = Services.io.newURI(url);
  Assert.equal(newURI.spec, url);
  Assert.equal(newURI.pathQueryRef, "/folder/file?/.");
  Assert.equal(newURI.resolve("./file?/."), url);
}
