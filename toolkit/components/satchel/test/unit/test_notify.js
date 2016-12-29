/*
 * Test suite for satchel notifications
 *
 * Tests notifications dispatched when modifying form history.
 *
 */

var expectedNotification;
var expectedData;

var TestObserver = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    do_check_eq(topic, "satchel-storage-changed");
    do_check_eq(data, expectedNotification);

    switch (data) {
        case "formhistory-add":
        case "formhistory-update":
            do_check_true(subject instanceof Ci.nsISupportsString);
            do_check_true(isGUID.test(subject.toString()));
            break;
        case "formhistory-remove":
            do_check_eq(null, subject);
            break;
        default:
            do_throw("Unhandled notification: " + data + " / " + topic);
    }

    expectedNotification = null;
    expectedData = null;
  }
};

var testIterator = null;

function run_test() {
  do_test_pending();
  testIterator = run_test_steps();
  testIterator.next();
}

function next_test()
{
  testIterator.next();
}

function* run_test_steps() {

try {

var testnum = 0;
var testdesc = "Setup of test form history entries";

var entry1 = ["entry1", "value1"];

/* ========== 1 ========== */
testnum = 1;
testdesc = "Initial connection to storage module"

yield updateEntry("remove", null, null, next_test);
yield countEntries(null, null, function(num) { do_check_false(num, "Checking initial DB is empty"); next_test(); });

// Add the observer
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
os.addObserver(TestObserver, "satchel-storage-changed", false);

/* ========== 2 ========== */
testnum++;
testdesc = "addEntry";

expectedNotification = "formhistory-add";
expectedData = entry1;

yield updateEntry("add", entry1[0], entry1[1], next_test);
do_check_eq(expectedNotification, null); // check that observer got a notification

yield countEntries(entry1[0], entry1[1], function(num) { do_check_true(num > 0); next_test(); });

/* ========== 3 ========== */
testnum++;
testdesc = "modifyEntry";

expectedNotification = "formhistory-update";
expectedData = entry1;
// will update previous entry
yield updateEntry("update", entry1[0], entry1[1], next_test);
yield countEntries(entry1[0], entry1[1], function(num) { do_check_true(num > 0); next_test(); });

do_check_eq(expectedNotification, null);

/* ========== 4 ========== */
testnum++;
testdesc = "removeEntry";

expectedNotification = "formhistory-remove";
expectedData = entry1;
yield updateEntry("remove", entry1[0], entry1[1], next_test);

do_check_eq(expectedNotification, null);
yield countEntries(entry1[0], entry1[1], function(num) { do_check_false(num, "doesn't exist after remove"); next_test(); });

/* ========== 5 ========== */
testnum++;
testdesc = "removeAllEntries";

expectedNotification = "formhistory-remove";
expectedData = null; // no data expected
yield updateEntry("remove", null, null, next_test);

do_check_eq(expectedNotification, null);

/* ========== 6 ========== */
testnum++;
testdesc = "removeAllEntries (again)";

expectedNotification = "formhistory-remove";
expectedData = null;
yield updateEntry("remove", null, null, next_test);

do_check_eq(expectedNotification, null);

/* ========== 7 ========== */
testnum++;
testdesc = "removeEntriesForName";

expectedNotification = "formhistory-remove";
expectedData = "field2";
yield updateEntry("remove", null, "field2", next_test);

do_check_eq(expectedNotification, null);

/* ========== 8 ========== */
testnum++;
testdesc = "removeEntriesByTimeframe";

expectedNotification = "formhistory-remove";
expectedData = [10, 99999999999];

yield FormHistory.update({ op: "remove", firstUsedStart: expectedData[0], firstUsedEnd: expectedData[1] },
                         { handleCompletion(reason) { if (!reason) next_test() },
                           handleErrors(error) {
                             do_throw("Error occurred updating form history: " + error);
                           }
                         });

do_check_eq(expectedNotification, null);

os.removeObserver(TestObserver, "satchel-storage-changed", false);

do_test_finished();

} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
}
