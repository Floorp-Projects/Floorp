function run_test() {
  var test_path = do_get_file("../unit/test_predictor.js").path.replace(/\\/g, "/");
  load(test_path);
  do_load_child_test_harness();
  do_test_pending();
  sendCommand("load(\"" + test_path + "\");", function () {
    sendCommand("predictor = Cc[\"@mozilla.org/network/predictor;1\"].getService(Ci.nsINetworkPredictor);", function() {
      run_test_real();
      do_test_finished();
    });
  });
}
