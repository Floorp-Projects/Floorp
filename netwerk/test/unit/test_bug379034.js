"use strict";

function run_test() {
  const ios = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );

  var base = ios.newURI("http://localhost/bug379034/index.html");

  var uri = ios.newURI("http:a.html", null, base);
  Assert.equal(uri.spec, "http://localhost/bug379034/a.html");

  uri = ios.newURI("HtTp:b.html", null, base);
  Assert.equal(uri.spec, "http://localhost/bug379034/b.html");

  uri = ios.newURI("https:c.html", null, base);
  Assert.equal(uri.spec, "https://c.html/");

  uri = ios.newURI("./https:d.html", null, base);
  Assert.equal(uri.spec, "http://localhost/bug379034/https:d.html");
}
