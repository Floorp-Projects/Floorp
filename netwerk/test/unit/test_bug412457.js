function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  // check if hostname is unescaped before applying IDNA
  var newURI = ios.newURI("http://\u5341%2ecom/");
  Assert.equal(newURI.asciiHost, "xn--kkr.com");

  // escaped UTF8
  newURI.spec = "http://%e5%8d%81.com";
  Assert.equal(newURI.asciiHost, "xn--kkr.com");

  // There should be only allowed characters in hostname after
  // unescaping and attempting to apply IDNA. "\x80" is illegal in
  // UTF-8, so IDNA fails, and 0x80 is illegal in DNS too.
  Assert.throws(() => { newURI.spec = "http://%80.com"; }, "illegal UTF character");

  // test parsing URL with all possible host terminators
  newURI.spec = "http://example.com?foo";
  Assert.equal(newURI.asciiHost, "example.com");

  newURI.spec = "http://example.com#foo";
  Assert.equal(newURI.asciiHost, "example.com");

  newURI.spec = "http://example.com:80";
  Assert.equal(newURI.asciiHost, "example.com");

  newURI.spec = "http://example.com/foo";
  Assert.equal(newURI.asciiHost, "example.com");

  // Characters that are invalid in the host, shouldn't be decoded.
  newURI.spec = "http://example.com%3ffoo";
  Assert.equal(newURI.asciiHost, "example.com%3ffoo");
  newURI.spec = "http://example.com%23foo";
  Assert.equal(newURI.asciiHost, "example.com%23foo");
  newURI.spec = "http://example.com%3bfoo";
  Assert.equal(newURI.asciiHost, "example.com%3bfoo");
  newURI.spec = "http://example.com%3a80";
  Assert.equal(newURI.asciiHost, "example.com%3a80");
  newURI.spec = "http://example.com%2ffoo";
  Assert.equal(newURI.asciiHost, "example.com%2ffoo");
  newURI.spec = "http://example.com%00";
  Assert.equal(newURI.asciiHost, "example.com%00");
}