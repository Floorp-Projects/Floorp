Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  let manifest = do_get_file('testcompwithaslr.manifest');
  Components.manager.autoRegister(manifest);
  do_check_false("{335fb596-e52d-418f-b01c-1bf16ce5e7e4}" in Components.classesByID);
}
