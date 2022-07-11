// Make sure we behave appropriately when asking for content-disposition

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const path = "data/test_bug589292.zip";

function run_test() {
  var spec =
    "jar:" + Services.io.newFileURI(do_get_file(path)).spec + "!/foo.txt";
  var channel = NetUtil.newChannel({
    uri: spec,
    loadUsingSystemPrincipal: true,
  });
  channel.open();
  try {
    channel.contentDisposition;
    Assert.ok(false, "The channel has content disposition?!");
  } catch (e) {
    // This is what we want to happen - there's no underlying channel, so no
    // content-disposition header is available
    Assert.ok(true, "How are you reading this?!");
  }
}
