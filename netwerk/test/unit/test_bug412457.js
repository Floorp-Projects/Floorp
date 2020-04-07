"use strict";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

  // check if hostname is unescaped before applying IDNA
  var newURI = ios.newURI("http://\u5341%2ecom/");
  Assert.equal(newURI.asciiHost, "xn--kkr.com");

  // escaped UTF8
  newURI = newURI
    .mutate()
    .setSpec("http://%e5%8d%81.com")
    .finalize();
  Assert.equal(newURI.asciiHost, "xn--kkr.com");

  // There should be only allowed characters in hostname after
  // unescaping and attempting to apply IDNA. "\x80" is illegal in
  // UTF-8, so IDNA fails, and 0x80 is illegal in DNS too.
  Assert.throws(
    () => {
      newURI = newURI
        .mutate()
        .setSpec("http://%80.com")
        .finalize();
    },
    /NS_ERROR_UNEXPECTED/,
    "illegal UTF character"
  );

  // test parsing URL with all possible host terminators
  newURI = newURI
    .mutate()
    .setSpec("http://example.com?foo")
    .finalize();
  Assert.equal(newURI.asciiHost, "example.com");

  newURI = newURI
    .mutate()
    .setSpec("http://example.com#foo")
    .finalize();
  Assert.equal(newURI.asciiHost, "example.com");

  newURI = newURI
    .mutate()
    .setSpec("http://example.com:80")
    .finalize();
  Assert.equal(newURI.asciiHost, "example.com");

  newURI = newURI
    .mutate()
    .setSpec("http://example.com/foo")
    .finalize();
  Assert.equal(newURI.asciiHost, "example.com");

  // Characters that are invalid in the host
  Assert.throws(
    () => {
      newURI = newURI
        .mutate()
        .setSpec("http://example.com%3ffoo")
        .finalize();
    },
    /NS_ERROR_MALFORMED_URI/,
    "bad escaped character"
  );
  Assert.throws(
    () => {
      newURI = newURI
        .mutate()
        .setSpec("http://example.com%23foo")
        .finalize();
    },
    /NS_ERROR_MALFORMED_URI/,
    "bad escaped character"
  );
  Assert.throws(
    () => {
      newURI = newURI
        .mutate()
        .setSpec("http://example.com%3bfoo")
        .finalize();
    },
    /NS_ERROR_MALFORMED_URI/,
    "bad escaped character"
  );
  Assert.throws(
    () => {
      newURI = newURI
        .mutate()
        .setSpec("http://example.com%3a80")
        .finalize();
    },
    /NS_ERROR_MALFORMED_URI/,
    "bad escaped character"
  );
  Assert.throws(
    () => {
      newURI = newURI
        .mutate()
        .setSpec("http://example.com%2ffoo")
        .finalize();
    },
    /NS_ERROR_MALFORMED_URI/,
    "bad escaped character"
  );
  Assert.throws(
    () => {
      newURI = newURI
        .mutate()
        .setSpec("http://example.com%00")
        .finalize();
    },
    /NS_ERROR_MALFORMED_URI/,
    "bad escaped character"
  );
}
