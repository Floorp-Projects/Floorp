/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;
var fac;
var prefs;

let numRecords, timeGroupingSize, now;

const DEFAULT_EXPIRE_DAYS = 180;

function padLeft(number, length) {
    var str = number + '';
    while (str.length < length)
        str = '0' + str;
    return str;
}

function getFormExpiryDays() {
    if (prefs.prefHasUserValue("browser.formfill.expire_days"))
        return prefs.getIntPref("browser.formfill.expire_days");
    else
        return DEFAULT_EXPIRE_DAYS;
}

function run_test() {
    // ===== test init =====
    var testfile = do_get_file("formhistory_autocomplete.sqlite");
    var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

    // Cleanup from any previous tests or failures.
    var destFile = profileDir.clone();
    destFile.append("formhistory.sqlite");
    if (destFile.exists())
      destFile.remove(false);

    testfile.copyTo(profileDir, "formhistory.sqlite");

    fac = Cc["@mozilla.org/satchel/form-autocomplete;1"].
          getService(Ci.nsIFormAutoComplete);
    prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);

    timeGroupingSize = prefs.getIntPref("browser.formfill.timeGroupingSize") * 1000 * 1000;

    run_next_test();
}

add_test(function test0() {
    var maxTimeGroupings = prefs.getIntPref("browser.formfill.maxTimeGroupings");
    var bucketSize = prefs.getIntPref("browser.formfill.bucketSize");

    // ===== Tests with constant timesUsed and varying lastUsed date =====
    // insert 2 records per bucket to check alphabetical sort within
    now = 1000 * Date.now();
    numRecords = Math.ceil(maxTimeGroupings / bucketSize) * 2;

    let changes = [ ];
    for (let i = 0; i < numRecords; i+=2) {
        let useDate = now - (i/2 * bucketSize * timeGroupingSize);

        changes.push({ op : "add", fieldname: "field1", value: "value" + padLeft(numRecords - 1 - i, 2),
                       timesUsed: 1, firstUsed: useDate, lastUsed: useDate });
        changes.push({ op : "add", fieldname: "field1", value: "value" + padLeft(numRecords - 2 - i, 2),
                       timesUsed: 1, firstUsed: useDate, lastUsed: useDate });
    }

    updateFormHistory(changes, run_next_test);
});

add_test(function test1() {
    do_log_info("Check initial state is as expected");

    countEntries(null, null, function (count) {
      countEntries("field1", null, function (count) {
        do_check_true(count > 0);
        run_next_test();
      });
    });
});

add_test(function test2() {
    do_log_info("Check search contains all entries");

    fac.autoCompleteSearchAsync("field1", "", null, null, {
        onSearchCompletion : function(aResults) {
            do_check_eq(numRecords, aResults.matchCount);
            run_next_test();
        }
    });
});

add_test(function test3() {
    do_log_info("Check search result ordering with empty search term");

    let lastFound = numRecords;
    fac.autoCompleteSearchAsync("field1", "", null, null, {
        onSearchCompletion : function(aResults) {
            for (let i = 0; i < numRecords; i+=2) {
                do_check_eq(parseInt(aResults.getValueAt(i + 1).substr(5), 10), --lastFound);
                do_check_eq(parseInt(aResults.getValueAt(i).substr(5), 10), --lastFound);
            }
            run_next_test();
        }
    });
});

add_test(function test4() {
    do_log_info("Check search result ordering with \"v\"");

    let lastFound = numRecords;
    fac.autoCompleteSearchAsync("field1", "v", null, null, {
        onSearchCompletion : function(aResults) {
            for (let i = 0; i < numRecords; i+=2) {
                do_check_eq(parseInt(aResults.getValueAt(i + 1).substr(5), 10), --lastFound);
                do_check_eq(parseInt(aResults.getValueAt(i).substr(5), 10), --lastFound);
            }
            run_next_test();
        }
    });
});

const timesUsedSamples = 20;

add_test(function test5() {
    do_log_info("Begin tests with constant use dates and varying timesUsed");

    let changes =  [];
    for (let i = 0; i < timesUsedSamples; i++) {
        let timesUsed = (timesUsedSamples - i);
        let change = { op : "add", fieldname: "field2", value: "value" + (timesUsedSamples - 1 -  i),
                       timesUsed: timesUsed * timeGroupingSize, firstUsed: now, lastUsed: now };
        changes.push(change);
    }
    updateFormHistory(changes, run_next_test);
});

add_test(function test6() {
    do_log_info("Check search result ordering with empty search term");

    let lastFound = timesUsedSamples;
    fac.autoCompleteSearchAsync("field2", "", null, null, {
        onSearchCompletion : function(aResults) {
            for (let i = 0; i < timesUsedSamples; i++) {
                do_check_eq(parseInt(aResults.getValueAt(i).substr(5)), --lastFound);
            }
            run_next_test();
        }
    });
});

add_test(function test7() {
    do_log_info("Check search result ordering with \"v\"");

    let lastFound = timesUsedSamples;
    fac.autoCompleteSearchAsync("field2", "v", null, null, {
        onSearchCompletion : function(aResults) {
            for (let i = 0; i < timesUsedSamples; i++) {
                do_check_eq(parseInt(aResults.getValueAt(i).substr(5)), --lastFound);
            }
            run_next_test();
        }
    });
});

add_test(function test8() {
    do_log_info("Check that \"senior citizen\" entries get a bonus (browser.formfill.agedBonus)");

    let agedDate = 1000 * (Date.now() - getFormExpiryDays() * 24 * 60 * 60 * 1000);

    let changes = [ ];
    changes.push({ op : "add", fieldname: "field3", value: "old but not senior",
                   timesUsed: 100, firstUsed: (agedDate + 60 * 1000 * 1000), lastUsed: now });
    changes.push({ op : "add", fieldname: "field3", value: "senior citizen",
                   timesUsed: 100, firstUsed: (agedDate - 60 * 1000 * 1000), lastUsed: now });
    updateFormHistory(changes, run_next_test);
});

add_test(function test9() {
    fac.autoCompleteSearchAsync("field3", "", null, null, {
        onSearchCompletion : function(aResults) {
            do_check_eq(aResults.getValueAt(0), "senior citizen");
            do_check_eq(aResults.getValueAt(1), "old but not senior");
            run_next_test();
        }
    });
});

add_test(function test10() {
    do_log_info("Check entries that are really old or in the future");

    let changes = [ ];
    changes.push({ op : "add", fieldname: "field4", value: "date of 0",
                   timesUsed: 1, firstUsed: 0, lastUsed: 0 });
    changes.push({ op : "add", fieldname: "field4", value: "in the future 1",
                   timesUsed: 1, firstUsed: 0, lastUsed: now * 2 });
    changes.push({ op : "add", fieldname: "field4", value: "in the future 2",
                   timesUsed: 1, firstUsed: now * 2, lastUsed: now * 2 });
    updateFormHistory(changes, run_next_test);
});

add_test(function test11() {
    fac.autoCompleteSearchAsync("field4", "", null, null, {
        onSearchCompletion : function(aResults) {
            do_check_eq(aResults.matchCount, 3);
            run_next_test();
        }
    });
});

let syncValues = ["sync1", "sync1a", "sync2", "sync3"]

add_test(function test12() {
    do_log_info("Check old synchronous api");

    let changes = [ ];
    for (let value of syncValues) {
      changes.push({ op : "add", fieldname: "field5", value: value });
    }
    updateFormHistory(changes, run_next_test);
});

add_test(function test13() {
  let autocompleteService = Cc["@mozilla.org/satchel/form-autocomplete;1"].getService(Ci.nsIFormAutoComplete);
  let results = autocompleteService.autoCompleteSearch("field5", "", null, null);
  do_check_eq(results.matchCount, syncValues.length, "synchronous matchCount");
  for (let i = 0; i < results.matchCount; i++) {
    do_check_eq(results.getValueAt(i), syncValues[i]);
  }

  let results = autocompleteService.autoCompleteSearch("field5", "sync1", null, null);
  do_check_eq(results.matchCount, 2, "synchronous matchCount");
  do_check_eq(results.getValueAt(0), "sync1");
  do_check_eq(results.getValueAt(1), "sync1a");
  run_next_test();
});
