var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
Cu.import("resource://gre/modules/NetUtil.jsm");

// Regression test for bug 370103 - crash when passing a null listener to
// nsIChannel.asyncOpen
function run_test() {
  // Compose the jar: url
  var ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
  var file = do_get_file("data/test_bug370103.jar");
  var url = ioService.newFileURI(file).spec;
  url = "jar:" + url + "!/test_bug370103";

  // Try opening channel with null listener
  var channel = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});

  var exception = false;
  try {
    channel.asyncOpen2(null);
  }
  catch(e) {
    exception = true;
  }

  Assert.ok(exception); // should throw exception instead of crashing
}
