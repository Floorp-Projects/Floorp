/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;
var fh;
var fac;
var prefs;

const DEFAULT_EXPIRE_DAYS = 180;

function countAllEntries() {
    let stmt = fh.DBConnection.createStatement("SELECT COUNT(*) as numEntries FROM moz_formhistory");
    do_check_true(stmt.executeStep());
    let numEntries = stmt.row.numEntries;
    stmt.finalize();
    return numEntries;
}

function padLeft(number, length) {
    var str = number + '';
    while (str.length < length)
        str = '0' + str;
    return str;
}

function getFormExpiryDays () {
    if (prefs.prefHasUserValue("browser.formfill.expire_days"))
        return prefs.getIntPref("browser.formfill.expire_days");
    else
        return DEFAULT_EXPIRE_DAYS;
}

function run_test() {
    try {

        // ===== test init =====
        var testfile = do_get_file("formhistory_autocomplete.sqlite");
        var profileDir = dirSvc.get("ProfD", Ci.nsIFile);

        // Cleanup from any previous tests or failures.
        var destFile = profileDir.clone();
        destFile.append("formhistory.sqlite");
        if (destFile.exists())
          destFile.remove(false);

        testfile.copyTo(profileDir, "formhistory.sqlite");

        fh = Cc["@mozilla.org/satchel/form-history;1"].
             getService(Ci.nsIFormHistory2);
        fac = Cc["@mozilla.org/satchel/form-autocomplete;1"].
              getService(Ci.nsIFormAutoComplete);
        prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);

        var timeGroupingSize = prefs.getIntPref("browser.formfill.timeGroupingSize") * 1000 * 1000;
        var maxTimeGroupings = prefs.getIntPref("browser.formfill.maxTimeGroupings");
        var bucketSize = prefs.getIntPref("browser.formfill.bucketSize");

        // ===== Tests with constant timesUsed and varying lastUsed date =====
        // insert 2 records per bucket to check alphabetical sort within
        var now = 1000 * Date.now();
        var numRecords = Math.ceil(maxTimeGroupings / bucketSize) * 2;

        fh.DBConnection.beginTransaction();
        for (let i = 0; i < numRecords; i+=2) {
            let useDate = now - (i/2 * bucketSize * timeGroupingSize);

            fh.DBConnection.executeSimpleSQL(
              "INSERT INTO moz_formhistory "+
              "(fieldname, value, timesUsed, firstUsed, lastUsed) " +
              "VALUES ("+
                  "'field1', " +
                  "'value" + padLeft(numRecords - 1 - i, 2) + "', " +
                  "1, " +
                  (useDate + 1) + ", " +
                  (useDate + 1) +
              ");");

              fh.DBConnection.executeSimpleSQL(
              "INSERT INTO moz_formhistory "+
              "(fieldname, value, timesUsed, firstUsed, lastUsed) " +
              "VALUES ("+
                  "'field1', " +
                  "'value" + padLeft(numRecords - 2 - i, 2) + "', " +
                  "1, " +
                  useDate + ", " +
                  useDate +
              ");");
        }
        fh.DBConnection.commitTransaction();

        // ===== 1 =====
        // Check initial state is as expected
        testnum++;
        do_check_true(fh.hasEntries);
        do_check_eq(numRecords, countAllEntries());
        do_check_true(fh.nameExists("field1"));

        // ===== 2 =====
        // Check search contains all entries
        testnum++;
        var results = fac.autoCompleteSearch("field1", "", null, null);
        do_check_eq(numRecords, results.matchCount);

        // ===== 3 =====
        // Check search result ordering with empty search term
        testnum++;
        results = fac.autoCompleteSearch("field1", "", null, null);
        let lastFound = numRecords;
        for (let i = 0; i < numRecords; i+=2) {
            do_check_eq(parseInt(results.getValueAt(i + 1).substr(5), 10), --lastFound);
            do_check_eq(parseInt(results.getValueAt(i).substr(5), 10), --lastFound);
        }

        // ===== 4 =====
        // Check search result ordering with "v"
        testnum++;
        results = fac.autoCompleteSearch("field1", "v", null, null);
        lastFound = numRecords;
        for (let i = 0; i < numRecords; i+=2) {
            do_check_eq(parseInt(results.getValueAt(i + 1).substr(5), 10), --lastFound);
            do_check_eq(parseInt(results.getValueAt(i).substr(5), 10), --lastFound);
        }

        // ===== Tests with constant use dates and varying timesUsed =====

        let timesUsedSamples = 20;
        fh.DBConnection.beginTransaction();
        for (let i = 0; i < timesUsedSamples; i++) {
            let timesUsed = (timesUsedSamples - i);
            fh.DBConnection.executeSimpleSQL(
              "INSERT INTO moz_formhistory "+
              "(fieldname, value, timesUsed, firstUsed, lastUsed) " +
              "VALUES ("+
                  "'field2', "+
                  "'value" + (timesUsedSamples - 1 -  i) + "', " +
                  timesUsed * timeGroupingSize + ", " +
                  now + ", " +
                  now +
              ");");
        }
        fh.DBConnection.commitTransaction();

        // ===== 5 =====
        // Check search result ordering with empty search term
        testnum++;
        results = fac.autoCompleteSearch("field2", "", null, null);
        lastFound = timesUsedSamples;
        for (let i = 0; i < timesUsedSamples; i++) {
            do_check_eq(parseInt(results.getValueAt(i).substr(5)), --lastFound);
        }

        // ===== 6 =====
        // Check search result ordering with "v"
        testnum++;
        results = fac.autoCompleteSearch("field2", "v", null, null);
        lastFound = timesUsedSamples;
        for (let i = 0; i < timesUsedSamples; i++) {
            do_check_eq(parseInt(results.getValueAt(i).substr(5)), --lastFound);
        }

        // ===== 7 =====
        // Check that "senior citizen" entries get a bonus (browser.formfill.agedBonus)
        testnum++;

        let agedDate = 1000 * (Date.now() - getFormExpiryDays() * 24 * 60 * 60 * 1000);
        fh.DBConnection.executeSimpleSQL(
          "INSERT INTO moz_formhistory "+
          "(fieldname, value, timesUsed, firstUsed, lastUsed) " +
          "VALUES ("+
              "'field3', " +
              "'old but not senior', " +
              "100, " +
              (agedDate + 60 * 1000 * 1000) + ", " +
              now +
          ");");
        fh.DBConnection.executeSimpleSQL(
          "INSERT INTO moz_formhistory "+
          "(fieldname, value, timesUsed, firstUsed, lastUsed) " +
          "VALUES ("+
              "'field3', " +
              "'senior citizen', " +
              "100, " +
              (agedDate - 60 * 1000 * 1000) + ", " +
              now +
          ");");

        results = fac.autoCompleteSearch("field3", "", null, null);
        do_check_eq(results.getValueAt(0), "senior citizen");
        do_check_eq(results.getValueAt(1), "old but not senior");

        // ===== 8 =====
        // Check entries that are really old or in the future
        testnum++;
        fh.DBConnection.executeSimpleSQL(
          "INSERT INTO moz_formhistory "+
          "(fieldname, value, timesUsed, firstUsed, lastUsed) " +
          "VALUES ("+
              "'field4', " +
              "'date of 0', " +
              "1, " +
              0 + ", " +
              0 +
          ");");

        fh.DBConnection.executeSimpleSQL(
          "INSERT INTO moz_formhistory "+
          "(fieldname, value, timesUsed, firstUsed, lastUsed) " +
          "VALUES ("+
              "'field4', " +
              "'in the future 1', " +
              "1, " +
              0 + ", " +
              (now * 2) +
          ");");

        fh.DBConnection.executeSimpleSQL(
          "INSERT INTO moz_formhistory "+
          "(fieldname, value, timesUsed, firstUsed, lastUsed) " +
          "VALUES ("+
              "'field4', " +
              "'in the future 2', " +
              "1, " +
              (now * 2) + ", " +
              (now * 2) +
          ");");

        results = fac.autoCompleteSearch("field4", "", null, null);
        do_check_eq(results.matchCount, 3);


    } catch (e) {
      throw "FAILED in test #" + testnum + " -- " + e;
    }
}
