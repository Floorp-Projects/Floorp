"use strict";

function run_test() {
  const ios = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );

  var uri = ios.newURI("http://foo.com/file.txt");
  uri = uri
    .mutate()
    .setPort(90)
    .finalize();
  Assert.equal(uri.hostPort, "foo.com:90");

  uri = ios.newURI("http://foo.com:10/file.txt");
  uri = uri
    .mutate()
    .setPort(500)
    .finalize();
  Assert.equal(uri.hostPort, "foo.com:500");

  uri = ios.newURI("http://foo.com:5000/file.txt");
  uri = uri
    .mutate()
    .setPort(20)
    .finalize();
  Assert.equal(uri.hostPort, "foo.com:20");

  uri = ios.newURI("http://foo.com:5000/file.txt");
  uri = uri
    .mutate()
    .setPort(-1)
    .finalize();
  Assert.equal(uri.hostPort, "foo.com");

  uri = ios.newURI("http://foo.com:5000/file.txt");
  uri = uri
    .mutate()
    .setPort(80)
    .finalize();
  Assert.equal(uri.hostPort, "foo.com");
}
