const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

// Regression test for bug 370103 - crash when passing a null listener to
// nsIChannel.asyncOpen
function run_test() {
  // Compose the jar: url
  var file = do_get_file("data/test_bug370103.jar");
  var url = Services.io.newFileURI(file).spec;
  url = "jar:" + url + "!/test_bug370103";

  // Try opening channel with null listener
  var channel = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  });

  var exception = false;
  try {
    channel.asyncOpen(null);
  } catch (e) {
    exception = true;
  }

  Assert.ok(exception); // should throw exception instead of crashing
}
