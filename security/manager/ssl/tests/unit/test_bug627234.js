var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

var gSTSService = Cc["@mozilla.org/stsservice;1"]
                  .getService(Ci.nsIStrictTransportSecurityService);

var gNextTest = null;

function cleanup() {
  leavePB();
  // (we have to remove any state added to the sts service so as to not muck
  // with other tests).
  var uri = Services.io.newURI("http://localhost", null, null);
  gSTSService.removeStsState(uri, privacyFlags());
}

function run_test() {
  do_test_pending();
  do_register_cleanup(cleanup);

  gNextTest = test_part1;
  enterPB();
}

var gInPrivate = false;
function enterPB() {
  gInPrivate = true;
  do_execute_soon(gNextTest);
}

function leavePB() {
  gInPrivate = false;
  Services.obs.notifyObservers(null, "last-pb-context-exited", null);
  do_execute_soon(gNextTest);
}

function privacyFlags() {
  return gInPrivate ? Ci.nsISocketProvider.NO_PERMANENT_STORAGE : 0;
}

function test_part1() {
  var uri = Services.io.newURI("https://localhost/img.png", null, null);
  gSTSService.processStsHeader(uri, "max-age=1000", privacyFlags());
  do_check_true(gSTSService.isStsHost("localhost", privacyFlags()));
  gNextTest = test_part2;
  leavePB();
}

function test_part2() {
  var uri = Services.io.newURI("https://localhost/img.png", null, null);
  gSTSService.processStsHeader(uri, "max-age=1000", privacyFlags());
  do_check_true(gSTSService.isStsHost("localhost", privacyFlags()));
  gNextTest = test_part3;
  enterPB();
}

function test_part3() {
  var uri = Services.io.newURI("https://localhost/img.png", null, null);
  gSTSService.processStsHeader(uri, "max-age=1000", privacyFlags());
  do_check_true(gSTSService.isStsHost("localhost", privacyFlags()));
  do_test_finished();
}
