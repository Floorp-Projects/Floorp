/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests whether
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");

var gTestserver = new HttpServer();
gTestserver.start(-1);
gPort = gTestserver.identity.primaryPort;
mapFile("/data/test_bug619730.xml", gTestserver);

function load_blocklist(file) {
  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" +
                             gPort + "/data/" + file);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  blocklist.notify(null);
}

var gSawGFX = false;
var gSawTest = false;

// Performs the initial setup
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  startupManager();

  do_test_pending();

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    do_check_true(aSubject instanceof AM_Ci.nsIDOMElement);
    do_check_eq(aSubject.getAttribute("testattr"), "GFX");
    do_check_eq(aSubject.childNodes.length, 2);
    gSawGFX = true;
  }, "blocklist-data-gfxItems", false);

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    do_check_true(aSubject instanceof AM_Ci.nsIDOMElement);
    do_check_eq(aSubject.getAttribute("testattr"), "FOO");
    do_check_eq(aSubject.childNodes.length, 3);
    gSawTest = true;
  }, "blocklist-data-testItems", false);

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    do_check_true(gSawGFX);
    do_check_true(gSawTest);

    gTestserver.stop(do_test_finished);
  }, "blocklist-data-fooItems", false);

  load_blocklist("test_bug619730.xml");
}
