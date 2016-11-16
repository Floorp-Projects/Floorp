Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function is_app_offline(appId) {
  let ioservice = Cc['@mozilla.org/network/io-service;1'].
    getService(Ci.nsIIOService);
  return ioservice.isAppOffline(appId);
}

var events_observed_no = 0;

// Holds the last observed app-offline event
var info = null;
function observer(aSubject, aTopic, aData) {
  events_observed_no++;
  info = aSubject.QueryInterface(Ci.nsIAppOfflineInfo);
  dump("ChildObserver - subject: {" + aSubject.appId + ", " + aSubject.mode + "} ");
}

// Add observer for the app-offline notification
function run_test() {
  Services.obs.addObserver(observer, "network:app-offline-status-changed", false);
}

// Chech that the app has the proper offline status
function check_status(appId, status)
{
  do_check_eq(is_app_offline(appId), status == Ci.nsIAppOfflineInfo.OFFLINE);
}

// Check that the app has the proper offline status
// and that the correct notification has been received
function check_notification_and_status(appId, status) {
  do_check_eq(info.appId, appId);
  do_check_eq(info.mode, status);
  do_check_eq(is_app_offline(appId), status == Ci.nsIAppOfflineInfo.OFFLINE);
}

// Remove the observer from the child process
function finished() {
  Services.obs.removeObserver(observer, "network:app-offline-status-changed");
  do_check_eq(events_observed_no, 2);
}
