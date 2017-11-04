"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");

// need profile so that PageThumbsStorageService can resolve the path to the underlying file
do_get_profile();

function run_test() {
  // check the protocol handler implements the correct interface
  let handler = Services.io.getProtocolHandler("moz-page-thumb");
  ok(handler instanceof Ci.nsIProtocolHandler,
     "moz-page-thumb handler provides a protocol handler interface");

  // and check that the error cases work as specified
  let badhost = Services.io.newURI("moz-page-thumb://wronghost/?url=http%3A%2F%2Fwww.mozilla.org%2F");
  Assert.throws(() => handler.newChannel(badhost), /NS_ERROR_NOT_AVAILABLE/i,
      "moz-page-thumb object with wrong host must not resolve to a file path");

  let badQuery = Services.io.newURI("moz-page-thumb://thumbnail/http%3A%2F%2Fwww.mozilla.org%2F");
  Assert.throws(() => handler.newChannel(badQuery), /NS_ERROR_MALFORMED_URI/i,
      "moz-page-thumb object with malformed query parameters must not resolve to a file path");

  let noURL = Services.io.newURI("moz-page-thumb://thumbnail/?badStuff");
  Assert.throws(() => handler.newChannel(noURL), /NS_ERROR_NOT_AVAILABLE/i,
      "moz-page-thumb object without a URL parameter must not resolve to a file path");
}
