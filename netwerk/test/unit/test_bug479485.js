const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var success = false;
  try {
    var newURI = ios.newURI("http://foo.com:invalid", null, null);
  }
  catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI when creating a new URI with :invalid as a port");

  success = false;
  newURI = ios.newURI("http://foo.com", null, null);
  try {
    newURI.spec = "http://foo.com:invalid";
  }
  catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success)
    do_throw("We didn't throw NS_ERROR_MALFORMED_URI when setting a spec of a URI with :invalid as a port");
}
