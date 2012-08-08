/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://testing-common/httpd.js");

const PREF_BLOCKLIST_LASTUPDATETIME   = "app.update.lastUpdateTime.blocklist-background-update-timer";
const PREF_BLOCKLIST_PINGCOUNTTOTAL   = "extensions.blocklist.pingCountTotal";
const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";

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

  gTestserver = new HttpServer();
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
  gExpectedQueryString = "invalid&invalid&invalid";
  notify_blocklist();
}

function test3() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - SECONDS_IN_DAY));
  gNextTest = test4;
  gExpectedQueryString = "2&2&1";
  notify_blocklist();
}

function test4() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, -1);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 2)));
  gNextTest = test5;
  gExpectedQueryString = "1&3&2";
  notify_blocklist();
}

function test5() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME, getNowInSeconds());
  gNextTest = test6;
  gExpectedQueryString = "invalid&invalid&0";
  notify_blocklist();
}

function test6() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 3)));
  gNextTest = test7;
  gExpectedQueryString = "2&4&3";
  notify_blocklist();
}

function test7() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, 2147483647);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 4)));
  gNextTest = test8;
  gExpectedQueryString = "2147483647&5&4";
  notify_blocklist();
}

function test8() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 5)));
  gNextTest = test9;
  gExpectedQueryString = "1&6&5";
  notify_blocklist();
}

function test9() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTTOTAL, 2147483647);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 6)));
  gNextTest = test10;
  gExpectedQueryString = "2&2147483647&6";
  notify_blocklist();
}

function test10() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 7)));
  gNextTest = test11;
  gExpectedQueryString = "3&1&7";
  notify_blocklist();
}

function test11() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, -1);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 8)));
  gNextTest = test12;
  gExpectedQueryString = "1&2&8";
  notify_blocklist();
}

function test12() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 9)));
  gNextTest = finish;
  gExpectedQueryString = "2&3&9";
  notify_blocklist();
}

function finish() {
  gTestserver.stop(do_test_finished);
}
