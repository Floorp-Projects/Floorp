// Make sure we behave appropriately when asking for content-disposition

const Cc = Components.classes;
const Ci = Components.interfaces;
const path = "data/test_bug589292.zip";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var spec = "jar:" + ios.newFileURI(do_get_file(path)).spec + "!/foo.txt";
  var channel = ios.newChannel(spec, null, null);
  instr = channel.open();
  var val;
  try {
    val = channel.contentDisposition;
    do_check_true(false, "The channel has content disposition?!");
  } catch (e) {
    // This is what we want to happen - there's no underlying channel, so no
    // content-disposition header is available
    do_check_true(true, "How are you reading this?!");
  }
}
