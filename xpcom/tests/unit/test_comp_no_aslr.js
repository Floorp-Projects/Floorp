Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  let manifest = do_get_file('testcompnoaslr.manifest');
  registerAppManifest(manifest);
  var sysInfo = Cc["@mozilla.org/system-info;1"].
                getService(Ci.nsIPropertyBag2);
  var ver = parseFloat(sysInfo.getProperty("version"));
  if (ver < 6.0) {
    // This is disabled on pre-Vista OSs.
    do_check_true("{335fb596-e52d-418f-b01c-1bf16ce5e7e4}" in Components.classesByID);
  } else {
    do_check_false("{335fb596-e52d-418f-b01c-1bf16ce5e7e4}" in Components.classesByID);
  }
}
