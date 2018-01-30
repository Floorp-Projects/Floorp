Cu.import("resource://gre/modules/Services.jsm");
function run_test() {
  Services.prefs.setBoolPref("network.cookie.ipc.sync", true);
  run_test_in_child("../unit/test_multipart_streamconv.js");
}
