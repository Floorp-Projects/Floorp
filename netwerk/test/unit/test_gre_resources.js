// test that things that are expected to be in gre-resources are still there

const Cc = Components.classes;
const Ci = Components.interfaces;
var ios = Cc["@mozilla.org/network/io-service;1"]. getService(Ci.nsIIOService);

function wrapInputStream(input)
{
  var nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;
  var factory = Components.classes["@mozilla.org/scriptableinputstream;1"];
  var wrapper = factory.createInstance(nsIScriptableInputStream);
  wrapper.init(input);
  return wrapper;
}

function check_file(file) {
  var channel = ios.newChannel("resource://gre-resources/"+file, null, null);
  try {
    let instr = wrapInputStream(channel.open());
    do_check_true(instr.read(1024).length > 0)
  } catch (e) {
    do_throw("Failed to read " + file + " from gre-resources:"+e)
  }
}

function run_test() {
  for each(let file in ["charsetData.properties"])
    check_file(file)
}
