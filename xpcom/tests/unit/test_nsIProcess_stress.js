function run_test() {
  set_process_running_environment();

  var file = get_test_program("TestQuickReturn");
  var tm = Cc["@mozilla.org/thread-manager;1"].getService();

  for (var i = 0; i < 1000; i++) {
    var process = Cc["@mozilla.org/process/util;1"].createInstance(
      Ci.nsIProcess
    );
    process.init(file);

    process.run(false, [], 0);

    try {
      process.kill();
    } catch (e) {}

    // We need to ensure that we process any events on the main thread -
    // this allow threads to clean up properly and avoid out of memory
    // errors during the test.
    tm.spinEventLoopUntilEmpty();
  }
}
