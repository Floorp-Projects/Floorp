const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/TelemetryLog.jsm", this);
Cu.import("resource://gre/modules/TelemetryPing.jsm", this);

function check_event(event, id, data)
{
  do_print("Checking message " + id);
  do_check_eq(event[0], id);
  do_check_true(event[1] > 0);

  if (data === undefined) {
    do_check_true(event.length == 2);
  } else {
    do_check_eq(event.length, data.length + 2);
    for (var i = 0; i < data.length; ++i) {
      do_check_eq(typeof(event[i + 2]), "string");
      do_check_eq(event[i + 2], data[i]);
    }
  }
}

function run_test()
{
  TelemetryLog.log("test1", ["val", 123, undefined]);
  TelemetryLog.log("test2", []);
  TelemetryLog.log("test3");

  var log = TelemetryPing.getPayload().log;
  do_check_eq(log.length, 3);
  check_event(log[0], "test1", ["val", "123", "undefined"]);
  check_event(log[1], "test2", []);
  check_event(log[2], "test3", undefined);
  do_check_true(log[0][1] <= log[1][1]);
  do_check_true(log[1][1] <= log[2][1]);
}
