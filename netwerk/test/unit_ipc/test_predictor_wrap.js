// This is the bit that runs in the parent process when the test begins. Here's
// what happens:
//
//  - Load the entire single-process test in the parent
//  - Setup the test harness within the child process
//  - Send a command to the child to have the single-process test loaded there, as well
//  - Send a command to the child to make the predictor available
//  - run_test_real in the parent
//  - run_test_real does a bunch of pref-setting and other fun things on the parent
//  - once all that's done, it calls run_next_test IN THE PARENT
//  - every time run_next_test is called, it is called IN THE PARENT
//  - the test that gets started then does any parent-side setup that's necessary
//  - once the parent-side setup is done, it calls continue_<testname> IN THE CHILD
//  - when the test is done running on the child, the child calls predictor.reset
//    this causes some code to be run on the parent which, when complete, sends an
//    obserer service notification IN THE PARENT, which causes run_next_test to be
//    called again, bumping us up to the top of the loop outlined here
//  - when the final test is done, cleanup happens IN THE PARENT and we're done
//
// This is a little confusing, but it's what we have to do in order to have some
// things that must run on the parent (the setup - opening cache entries, etc)
// but with most of the test running on the child (calls to the predictor api,
// verification, etc).
//
/* import-globals-from ../unit/test_predictor.js */

function run_test() {
  var test_path = do_get_file("../unit/test_predictor.js").path.replace(
    /\\/g,
    "/"
  );
  load(test_path);
  do_load_child_test_harness();
  do_test_pending();
  sendCommand('load("' + test_path + '");', function() {
    sendCommand(
      'predictor = Cc["@mozilla.org/network/predictor;1"].getService(Ci.nsINetworkPredictor);',
      function() {
        run_test_real();
        do_test_finished();
      }
    );
  });
}
