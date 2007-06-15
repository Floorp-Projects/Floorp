const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var newURI = ios.newURI("http://foo.com", null, null);

  var success = false;
  try {
    newURI.setSpec("http: //foo.com");
  }
  catch (e) {
    success = true;
  }
  if (!success)
    do_throw("We didn't throw when a space was passed in the hostname!");

  success = false;
  try {
    newURI.setHost(" foo.com");
  }
  catch (e) {
    success = true;
  }
  if (!success)
    do_throw("We didn't throw when a space was passed in the hostname!");
}
