function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);
    
  var test_port = function(port, exception_expected)
  {
    dump((port || "no port provided") + "\n");
    var exception_threw = false;
    try {
      var newURI = ios.newURI("http://foo.com"+port);
    }
    catch (e) {
      exception_threw = e.result == Cr.NS_ERROR_MALFORMED_URI;
    }
    if (exception_threw != exception_expected)
      do_throw("We did"+(exception_expected?"n't":"")+" throw NS_ERROR_MALFORMED_URI when creating a new URI with "+port+" as a port");
    do_check_eq(exception_threw, exception_expected);
  
    exception_threw = false;
    newURI = ios.newURI("http://foo.com");
    try {
      newURI.spec = "http://foo.com"+port;
    }
    catch (e) {
      exception_threw = e.result == Cr.NS_ERROR_MALFORMED_URI;
    }
    if (exception_threw != exception_expected)
      do_throw("We did"+(exception_expected?"n't":"")+" throw NS_ERROR_MALFORMED_URI when setting a spec of a URI with "+port+" as a port");
    do_check_eq(exception_threw, exception_expected);
  }
  
  test_port(":invalid", true);
  test_port(":-2", true);
  test_port(":-1", true);
  test_port(":0", false);
  test_port(":185891548721348172817857824356013651809236172635716571865023757816234081723451516780356", true);
  
  // Following 3 tests are all failing, we do not throw, although we parse the whole string and use only 5870 as a portnumber
  test_port(":5870:80", true);
  test_port(":5870-80", true);
  test_port(":5870+80", true);

  // Just a regression check
  test_port(":5870", false);
  test_port(":80", false);
  test_port("", false);
}
