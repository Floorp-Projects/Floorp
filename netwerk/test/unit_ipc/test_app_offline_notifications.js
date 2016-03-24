// Checks that app-offline notifications are received in both the parent
// and the child process, and that after receiving the notification
// isAppOffline returns the correct value


Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var test_index = 0;
var APP_ID = 42;
var events_observed_no = 0;

function set_app_offline(appId, offline) {
  let ioservice = Cc['@mozilla.org/network/io-service;1'].
    getService(Ci.nsIIOService);
  ioservice.setAppOffline(appId, offline);
}

function is_app_offline(appId) {
  let ioservice = Cc['@mozilla.org/network/io-service;1'].
    getService(Ci.nsIIOService);
  return ioservice.isAppOffline(appId);
}

// The expected offline status after running each function,
// and the next function that should be run.

//                       test0   test1   test2   test3   test4
let expected_offline = [ false,  true,   true,  false,  false];
let callbacks =        [ test1,  test2,  test3,  test4,  finished];

function observer(aSubject, aTopic, aData) {
  events_observed_no++;
  let info = aSubject.QueryInterface(Ci.nsIAppOfflineInfo);
  dump("ParentObserver - subject: {" + aSubject.appId + ", " + aSubject.mode + "} " +
       "topic: " + aTopic + "\n");

  // Check that the correct offline status is in place
  do_check_eq(is_app_offline(APP_ID), expected_offline[test_index]);

  // Execute the callback for the current test
  do_execute_soon(callbacks[test_index]);
}

function run_test() {
  Services.obs.addObserver(observer, "network:app-offline-status-changed", false);

  test_index = 0;
  do_check_eq(is_app_offline(APP_ID), expected_offline[test_index]) // The app should be online at first
  run_test_in_child("child_app_offline_notifications.js", test0);
}

// Check that the app is online by default in the child
function test0() {
  dump("parent: RUNNING: test0\n");
  test_index = 0;
  sendCommand('check_status('+APP_ID+','+Ci.nsIAppOfflineInfo.ONLINE+');\n', test1);
}

// Set the app OFFLINE
// Check that the notification is emmited in the parent process
// The observer function will execute test2 which does the check in the child
function test1() {
  dump("parent: RUNNING: test1\n");
  test_index = 1;
  set_app_offline(APP_ID, Ci.nsIAppOfflineInfo.OFFLINE);
}

// Checks that child process sees the app OFFLINE
function test2() {
  dump("parent: RUNNING: test2\n");
  test_index = 2;
  sendCommand('check_notification_and_status('+APP_ID+','+Ci.nsIAppOfflineInfo.OFFLINE+');\n', test3);
}

// Set the app ONLINE
// Chech that the notification is received in the parent
// The observer function will execute test3 and do the check in the child
function test3() {
  dump("parent: RUNNING: test3\n");
  test_index = 3;
  set_app_offline(APP_ID, Ci.nsIAppOfflineInfo.ONLINE);
}

// Chech that the app is back online
function test4() {
  dump("parent: RUNNING: test4\n");
  test_index = 4;
  sendCommand('check_notification_and_status('+APP_ID+','+Ci.nsIAppOfflineInfo.ONLINE+');\n', function() {
    // Send command to unregister observer on the child
    sendCommand('finished();\n', finished);
  });
}

// Remove observer and end test
function finished() {
  dump("parent: RUNNING: finished\n");
  Services.obs.removeObserver(observer, "network:app-offline-status-changed");
  do_check_eq(events_observed_no, 2);
  do_test_finished();
}
