/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://testing-common/httpd.js");

const PREF_BLOCKLIST_LASTUPDATETIME   = "app.update.lastUpdateTime.blocklist-background-update-timer";
const PREF_BLOCKLIST_PINGCOUNTTOTAL   = "extensions.blocklist.pingCountTotal";
const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";

const SECONDS_IN_DAY = 60 * 60 * 24;

var gTestserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

let resolveQuery;
gTestserver.registerPathHandler("/", function pathHandler(metadata, response) {
  resolveQuery(metadata.queryString);
});

async function checkQuery(expected) {
  let promise = new Promise(resolve => {
    resolveQuery = resolve;
  });

  Blocklist.notify();

  equal(await promise, expected, "Got expected blocklist query string");
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  gTestserver = new HttpServer();
  gTestserver.start(-1);
  gPort = gTestserver.identity.primaryPort;

  Services.prefs.setCharPref("extensions.blocklist.url",
                             "http://example.com/?%PING_COUNT%&%TOTAL_PING_COUNT%&%DAYS_SINCE_LAST_PING%");
});

function getNowInSeconds() {
  return Math.round(Date.now() / 1000);
}

add_task(async function test1() {
  await checkQuery("1&1&new");
});

add_task(async function test2() {
  await checkQuery("invalid&invalid&invalid");
});

add_task(async function test3() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - SECONDS_IN_DAY));
  await checkQuery("2&2&1");
});

add_task(async function test4() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, -1);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 2)));
  await checkQuery("1&3&2");
});

add_task(async function test5() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME, getNowInSeconds());
  await checkQuery("invalid&invalid&0");
});

add_task(async function test6() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 3)));
  await checkQuery("2&4&3");
});

add_task(async function test7() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, 2147483647);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 4)));
  await checkQuery("2147483647&5&4");
});

add_task(async function test8() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 5)));
  await checkQuery("1&6&5");
});

add_task(async function test9() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTTOTAL, 2147483647);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 6)));
  await checkQuery("2&2147483647&6");
});

add_task(async function test10() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 7)));
  await checkQuery("3&1&7");
});

add_task(async function test11() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTVERSION, -1);
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 8)));
  await checkQuery("1&2&8");
});

add_task(async function test12() {
  Services.prefs.setIntPref(PREF_BLOCKLIST_LASTUPDATETIME,
                            (getNowInSeconds() - (SECONDS_IN_DAY * 9)));
  await checkQuery("2&3&9");
});
