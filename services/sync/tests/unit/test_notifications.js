Cu.import("resource://services-sync/notifications.js");

function run_test() {
  var logStats = initTestLogging("Info");

  var blah = 0;

  function callback(i) {
    blah = i;
  }

  let button = new NotificationButton("label", "accessKey", callback);

  button.callback(5);

  do_check_eq(blah, 5);
  do_check_eq(logStats.errorsLogged, 0);

  function badCallback() {
    throw new Error("oops");
  }

  button = new NotificationButton("label", "accessKey", badCallback);

  try {
    button.callback();
  } catch (e) {
    do_check_eq(e.message, "oops");
  }

  do_check_eq(logStats.errorsLogged, 1);
}
