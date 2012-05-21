/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testnum = 0;
var fh;
var fac;
var prefs;

function countAllEntries() {
    let stmt = fh.DBConnection.createStatement("SELECT COUNT(*) as numEntries FROM moz_formhistory");
    do_check_true(stmt.executeStep());
    let numEntries = stmt.row.numEntries;
    stmt.finalize();
    return numEntries;
}

function do_AC_search(searchTerm, previousResult) {
    var duration = 0;
    var searchCount = 5;
    var tempPrevious = null;
    var startTime;
    for (var i = 0; i < searchCount; i++) {
        if (previousResult !== null)
            tempPrevious = fac.autoCompleteSearch("searchbar-history", previousResult, null, null);
        startTime = Date.now();
        results = fac.autoCompleteSearch("searchbar-history", searchTerm, null, tempPrevious);
        duration += Date.now() - startTime;
    }
    dump("[autoCompleteSearch][test " + testnum + "] for '" + searchTerm + "' ");
    if (previousResult !== null)
        dump("with '" + previousResult + "' previous result ");
    else
        dump("w/o previous result ");
    dump("took " + duration + " ms with " + results.matchCount + " matches.  ");
    dump("Average of " + Math.round(duration / searchCount) + " ms per search\n");
    return results;
}

function run_test() {
    try {

        // ===== test init =====
        var testfile = do_get_file("formhistory_1000.sqlite");
        var profileDir = dirSvc.get("ProfD", Ci.nsIFile);
        var results;

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

        timeGroupingSize = prefs.getIntPref("browser.formfill.timeGroupingSize") * 1000 * 1000;
        maxTimeGroupings = prefs.getIntPref("browser.formfill.maxTimeGroupings");
        bucketSize = prefs.getIntPref("browser.formfill.bucketSize");

        // ===== 1 =====
        // Check initial state is as expected
        testnum++;
        do_check_true(fh.hasEntries);
        do_check_eq(1000, countAllEntries());
        fac.autoCompleteSearch("searchbar-history", "zzzzzzzzzz", null, null); // warm-up search
        do_check_true(fh.nameExists("searchbar-history"));

        // ===== 2 =====
        // Search for '' with no previous result
        testnum++;
        results = do_AC_search("", null);
        do_check_true(results.matchCount > 0);

        // ===== 3 =====
        // Search for 'r' with no previous result
        testnum++;
        results = do_AC_search("r", null);
        do_check_true(results.matchCount > 0);

        // ===== 4 =====
        // Search for 'r' with '' previous result
        testnum++;
        results = do_AC_search("r", "");
        do_check_true(results.matchCount > 0);

        // ===== 5 =====
        // Search for 're' with no previous result
        testnum++;
        results = do_AC_search("re", null);
        do_check_true(results.matchCount > 0);

        // ===== 6 =====
        // Search for 're' with 'r' previous result
        testnum++;
        results = do_AC_search("re", "r");
        do_check_true(results.matchCount > 0);

        // ===== 7 =====
        // Search for 'rea' without previous result
        testnum++;
        results = do_AC_search("rea", null);
        let countREA = results.matchCount;

        // ===== 8 =====
        // Search for 'rea' with 're' previous result
        testnum++;
        results = do_AC_search("rea", "re");
        do_check_eq(countREA, results.matchCount);

        // ===== 9 =====
        // Search for 'real' with 'rea' previous result
        testnum++;
        results = do_AC_search("real", "rea");
        let countREAL = results.matchCount;
        do_check_true(results.matchCount <= countREA);

        // ===== 10 =====
        // Search for 'real' with 're' previous result
        testnum++;
        results = do_AC_search("real", "re");
        do_check_eq(countREAL, results.matchCount);

        // ===== 11 =====
        // Search for 'real' with no previous result
        testnum++;
        results = do_AC_search("real", null);
        do_check_eq(countREAL, results.matchCount);


    } catch (e) {
      throw "FAILED in test #" + testnum + " -- " + e;
    }
}
