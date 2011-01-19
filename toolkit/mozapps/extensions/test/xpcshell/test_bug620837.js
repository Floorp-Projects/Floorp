/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

do_load_httpd_js();

const PREF_BLOCKLIST_LASTUPDATETIME = "app.update.lastUpdateTime.blocklist-background-update-timer";
const PREF_BLOCKLIST_PINGCOUNT      = "extensions.blocklist.pingCount";

const SECONDS_IN_DAY = 60 * 60 * 24;

var gExpectedQueryString = null;
var gNextTest = null;
var gTestserver = null;

function notify_blocklist() {
  var blocklist = AM_Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(AM_Ci.nsITimerCallback);
  blocklist.notify(null);
}

function pathHandler(metadata, response) {
  do_check_eq(metadata.queryString, gExpectedQueryString);
  gNextTest();
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  gTestserver = new nsHttpServer();
  gTestserver.registerPathHandler("/", pathHandler);
  gTestserver.start(4444);

  Services.prefs.setCharPref("extensions.blocklist.url",
                             "http://localhost:4444/?%PING_COUNT%&%TOTAL_PING_COUNT%&%DAYS_SINCE_LAST_PING%");

  do_test_pending();
  test1();
}

function getNowInSeconds() {
  return Math.round(Date.now() / 1000);
}

function test1() {
  gNextTest = test2;
  gExpectedQueryString = "1&1&new";
  notify_blocklist();
}

function test2() {
  gNextTest = test3;
  gExpectedQueryString = "2&2&invalid";
  notify_blocklist();
}

function test3() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - SECONDS_IN_DAY));
  gNextTest = test4;
  gExpectedQueryString = "3&3&1";
  notify_blocklist();
}

function test4() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNT, -1);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 2)));
  gNextTest = test5;
  gExpectedQueryString = "1&4&reset";
  notify_blocklist();
}

function test5() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 3)));
  gNextTest = finish;
  gExpectedQueryString = "2&5&3";
  notify_blocklist();
}

function finish() {
  gTestserver.stop(do_test_finished);
}
