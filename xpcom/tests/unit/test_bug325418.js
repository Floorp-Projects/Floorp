const Cc = Components.classes;
const Ci = Components.interfaces;

// 5 seconds.
const kExpectedDelay1 = 5;
// 1 second.
const kExpectedDelay2 = 1;

var gStartTime1;
var gStartTime2;
var timer;

var observer1 = {
  observe: function observeTC1(subject, topic, data) {
    if (topic == "timer-callback") {
      // Stop timer, so it doesn't repeat (if test runs slowly).
      timer.cancel();

      // Actual delay may not be exact, so convert to seconds and round.
      do_check_eq(Math.round((Date.now() - gStartTime1) / 1000),
                  kExpectedDelay1);

      timer = null;

      do_print("1st timer triggered (before being cancelled). Should not have happened!");
      do_check_true(false);
    }
  }
};

var observer2 = {
  observe: function observeTC2(subject, topic, data) {
    if (topic == "timer-callback") {
      // Stop timer, so it doesn't repeat (if test runs slowly).
      timer.cancel();

      // Actual delay may not be exact, so convert to seconds and round.
      do_check_eq(Math.round((Date.now() - gStartTime2) / 1000),
                  kExpectedDelay2);

      timer = null;

      do_test_finished();
    }
  }
};

function run_test() {
  do_test_pending();

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

  // Initialize the timer (with some delay), then cancel it.
  gStartTime1 = Date.now();
  timer.init(observer1, kExpectedDelay1 * 1000, timer.TYPE_REPEATING_PRECISE);
  timer.cancel();

  // Re-initialize the timer (with a different delay).
  gStartTime2 = Date.now();
  timer.init(observer2, kExpectedDelay2 * 1000, timer.TYPE_REPEATING_PRECISE);
}
