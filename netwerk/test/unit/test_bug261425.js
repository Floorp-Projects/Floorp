function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var newURI = ios.newURI("http://foo.com");

  var success = false;
  try {
    newURI.spec = "http: //foo.com";
  }
  catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI when a space was passed in the hostname!");

  success = false;
  try {
    newURI.host = " foo.com";
  }
  catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI when a space was passed in the hostname!");
}
