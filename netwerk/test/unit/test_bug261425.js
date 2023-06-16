"use strict";

function run_test() {
  var newURI = Services.io.newURI("http://foo.com");

  var success = false;
  try {
    newURI = newURI.mutate().setSpec("http: //foo.com").finalize();
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success) {
    do_throw(
      "We didn't throw NS_ERROR_MALFORMED_URI when a space was passed in the hostname!"
    );
  }

  success = false;
  try {
    newURI.mutate().setHost(" foo.com").finalize();
  } catch (e) {
    success = e.result == Cr.NS_ERROR_MALFORMED_URI;
  }
  if (!success) {
    do_throw(
      "We didn't throw NS_ERROR_MALFORMED_URI when a space was passed in the hostname!"
    );
  }
}
