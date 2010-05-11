/*
 * Test suite for satchel notifications
 *
 * Tests notifications dispatched when modifying form history.
 *
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var expectedNotification;
var expectedBeforeNotification = null;
var expectedData;

var TestObserver = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  observe : function (subject, topic, data) {
    do_check_eq(topic, "satchel-storage-changed");

    // ensure that the "before-" notification comes before the other
    dump(expectedBeforeNotification + " : " + expectedNotification + "\n");
    if (!expectedBeforeNotification)
        do_check_eq(data, expectedNotification);
    else
        do_check_eq(data, expectedBeforeNotification);

    switch (data) {
        case "addEntry":
            do_check_true(subject instanceof Ci.nsIMutableArray);
            do_check_eq(expectedData[0], subject.queryElementAt(0, Ci.nsISupportsString));
            do_check_eq(expectedData[1], subject.queryElementAt(1, Ci.nsISupportsString));
            do_check_true(isGUID.test(subject.queryElementAt(2, Ci.nsISupportsString).toString()));
            break;
        case "modifyEntry":
            do_check_true(subject instanceof Ci.nsIMutableArray);
            do_check_eq(expectedData[0], subject.queryElementAt(0, Ci.nsISupportsString));
            do_check_eq(expectedData[1], subject.queryElementAt(1, Ci.nsISupportsString));
            do_check_true(isGUID.test(subject.queryElementAt(2, Ci.nsISupportsString).toString()));
            break;
        case "before-removeEntry":
        case "removeEntry":
            do_check_true(subject instanceof Ci.nsIMutableArray);
            do_check_eq(expectedData[0], subject.queryElementAt(0, Ci.nsISupportsString));
            do_check_eq(expectedData[1], subject.queryElementAt(1, Ci.nsISupportsString));
            do_check_true(isGUID.test(subject.queryElementAt(2, Ci.nsISupportsString).toString()));
            break;
        case "before-removeAllEntries":
        case "removeAllEntries":
            do_check_eq(subject, expectedData);
            break;
        case "before-removeEntriesForName":
        case "removeEntriesForName":
            do_check_true(subject instanceof Ci.nsISupportsString);
            do_check_eq(subject, expectedData);
            break;
        case "before-removeEntriesByTimeframe":
        case "removeEntriesByTimeframe":
            do_check_true(subject instanceof Ci.nsIMutableArray);
            do_check_eq(expectedData[0], subject.queryElementAt(0, Ci.nsISupportsPRInt64));
            do_check_eq(expectedData[1], subject.queryElementAt(1, Ci.nsISupportsPRInt64));
            break;
        case "before-expireOldEntries":
        case "expireOldEntries":
            do_check_true(subject instanceof Ci.nsISupportsPRInt64);
            do_check_true(subject.data > 0);
            break;
        default:
            do_throw("Unhandled notification: " + data + " / " + topic);
    }
    // ensure a duplicate is flagged as unexpected
    if (expectedBeforeNotification) {
        expectedBeforeNotification = null;
    } else {
        expectedNotification = null;
        expectedData = null;
    }
  }
};

function countAllEntries() {
    let stmt = fh.DBConnection.createStatement("SELECT COUNT(*) as numEntries FROM moz_formhistory");
    do_check_true(stmt.step());
    let numEntries = stmt.row.numEntries;
    stmt.finalize();
    return numEntries;
}

function triggerExpiration() {
    // We can't easily fake a "daily idle" event, so for testing purposes form
    // history listens for another notification to trigger an immediate
    // expiration.
    var os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    os.notifyObservers(null, "formhistory-expire-now", null);
}

function run_test() {

try {

var testnum = 0;
var testdesc = "Setup of test form history entries";
fh = Cc["@mozilla.org/satchel/form-history;1"].
     getService(Ci.nsIFormHistory2);

do_check_true(fh != null);

var entry1 = ["entry1", "value1"];
var entry2 = ["entry2", "value2"];


// Add the observer
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
os.addObserver(TestObserver, "satchel-storage-changed", false);


/* ========== 1 ========== */
var testnum = 1;
var testdesc = "Initial connection to storage module"

fh.DBConnection.executeSimpleSQL("DELETE FROM moz_formhistory");
do_check_eq(countAllEntries(), 0, "Checking initial DB is empty");

/* ========== 2 ========== */
testnum++;
testdesc = "addEntry";

expectedNotification = "addEntry";
expectedData = entry1;
fh.addEntry(entry1[0], entry1[1]);
do_check_true(fh.entryExists(entry1[0], entry1[1]));
do_check_eq(expectedNotification, null); // check that observer got a notification

/* ========== 3 ========== */
testnum++;
testdesc = "modifyEntry";

expectedNotification = "modifyEntry";
expectedData = entry1;
fh.addEntry(entry1[0], entry1[1]); // will update previous entry
do_check_eq(expectedNotification, null);

/* ========== 4 ========== */
testnum++;
testdesc = "removeEntry";

expectedNotification = "removeEntry";
expectedBeforeNotification = "before-" + expectedNotification;
expectedData = entry1;
fh.removeEntry(entry1[0], entry1[1]);
do_check_eq(expectedNotification, null);
do_check_eq(expectedBeforeNotification, null);
do_check_true(!fh.entryExists(entry1[0], entry1[1]));

/* ========== 5 ========== */
testnum++;
testdesc = "removeAllEntries";

expectedNotification = "removeAllEntries";
expectedBeforeNotification = "before-" + expectedNotification;
expectedData = null; // no data expected
fh.removeAllEntries();
do_check_eq(expectedNotification, null);
do_check_eq(expectedBeforeNotification, null);

/* ========== 6 ========== */
testnum++;
testdesc = "removeAllEntries (again)";

expectedNotification = "removeAllEntries";
expectedBeforeNotification = "before-" + expectedNotification;
expectedData = null;
fh.removeAllEntries();
do_check_eq(expectedNotification, null);
do_check_eq(expectedBeforeNotification, null);

/* ========== 7 ========== */
testnum++;
testdesc = "removeEntriesForName";

expectedNotification = "removeEntriesForName";
expectedBeforeNotification = "before-" + expectedNotification;
expectedData = "field2";
fh.removeEntriesForName("field2");
do_check_eq(expectedNotification, null);
do_check_eq(expectedBeforeNotification, null);

/* ========== 8 ========== */
testnum++;
testdesc = "removeEntriesByTimeframe";

expectedNotification = "removeEntriesByTimeframe";
expectedBeforeNotification = "before-" + expectedNotification;
expectedData = [10, 99999999999];
fh.removeEntriesByTimeframe(expectedData[0], expectedData[1]);
do_check_eq(expectedNotification, null);
do_check_eq(expectedBeforeNotification, null);

/* ========== 9 ========== */
testnum++;
testdesc = "expireOldEntries";

expectedNotification = "expireOldEntries";
expectedBeforeNotification = "before-" + expectedNotification;
expectedData = null; // TestObserver checks expiryDate > 0
triggerExpiration();
do_check_eq(expectedNotification, null);
do_check_eq(expectedBeforeNotification, null);

} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
};
