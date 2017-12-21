var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test() {
  function makeURI(aURLSpec, aCharset) {
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    return ios.newURI(aURLSpec, aCharset);
  }

  var httpURI = makeURI("http://foo.com");
  Assert.equal(-1, httpURI.port);

  // Setting to default shouldn't cause a change
  httpURI.port = 80;
  Assert.equal(-1, httpURI.port);
  
  // Setting to default after setting to non-default shouldn't cause a change (bug 403480)
  httpURI.port = 123;
  Assert.equal(123, httpURI.port);
  httpURI.port = 80;
  Assert.equal(-1, httpURI.port);
  Assert.ok(!/80/.test(httpURI.spec));

  // URL parsers shouldn't set ports to default value (bug 407538)
  httpURI.spec = "http://foo.com:81";
  Assert.equal(81, httpURI.port);
  httpURI.spec = "http://foo.com:80";
  Assert.equal(-1, httpURI.port);
  Assert.ok(!/80/.test(httpURI.spec));

  httpURI = makeURI("http://foo.com");
  Assert.equal(-1, httpURI.port);
  Assert.ok(!/80/.test(httpURI.spec));

  httpURI = makeURI("http://foo.com:80");
  Assert.equal(-1, httpURI.port);
  Assert.ok(!/80/.test(httpURI.spec));

  httpURI = makeURI("http://foo.com:80");
  Assert.equal(-1, httpURI.port);
  Assert.ok(!/80/.test(httpURI.spec));

  httpURI = makeURI("https://foo.com");
  Assert.equal(-1, httpURI.port);
  Assert.ok(!/443/.test(httpURI.spec));

  httpURI = makeURI("https://foo.com:443");
  Assert.equal(-1, httpURI.port);
  Assert.ok(!/443/.test(httpURI.spec));

  // XXX URL parsers shouldn't set ports to default value, even when changing scheme?
  // not really possible given current nsIURI impls
  //httpURI.spec = "https://foo.com:443";
  //do_check_eq(-1, httpURI.port);
}
