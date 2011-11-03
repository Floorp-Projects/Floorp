const Cc = Components.classes;
const Ci = Components.interfaces;

var timer;
const start_time = (new Date()).getTime();
const expected_time = 1;

var observer = {
  observe: function(subject, topic, data) {
    if (topic == "timer-callback") {
      var stop_time = (new Date()).getTime();
      // expected time may not be exact so convert to seconds and round down.
      var result = Math.round((stop_time - start_time) / 1000);
      do_check_true(result, expected_time);

      do_test_finished();

      timer.cancel();
      timer = null;
    }
  }
};

function run_test() {
  do_test_pending();

  timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  // Start a 5 second timer, than cancel it and start a 1 second timer.
  timer.init(observer, 5000, timer.TYPE_REPEATING_PRECISE);
  timer.cancel();
  timer.init(observer, 1000, timer.TYPE_REPEATING_PRECISE);
}