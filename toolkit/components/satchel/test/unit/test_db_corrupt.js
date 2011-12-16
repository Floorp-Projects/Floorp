/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let bakFile;

function run_test() {
  // ===== test init =====
  let testfile = do_get_file("formhistory_CORRUPT.sqlite");
  let profileDir = dirSvc.get("ProfD", Ci.nsIFile);

  // Cleanup from any previous tests or failures.
  let destFile = profileDir.clone();
  destFile.append("formhistory.sqlite");
  if (destFile.exists())
    destFile.remove(false);

  bakFile = profileDir.clone();
  bakFile.append("formhistory.sqlite.corrupt");
  if (bakFile.exists())
    bakFile.remove(false);

  testfile.copyTo(profileDir, "formhistory.sqlite");
  run_next_test();
}

add_test(function test_corruptFormHistoryDB_lazyCorruptInit1() {
  do_log_info("ensure FormHistory backs up a corrupt DB on initialization.");

  // DB init is done lazily so the DB shouldn't be created yet.
  do_check_false(bakFile.exists());
  // Doing any request to the DB should create it.
  FormHistory.count({}, {
    onSuccess : function(aNumEntries) {
      run_next_test();
    },
    onFailure : function(aError) {
      do_throw("DB initialization failed.");
    }
  });
});

add_test(function test_corruptFormHistoryDB_lazyCorruptInit2() {
  do_check_true(bakFile.exists());
  bakFile.remove(false);
  run_next_test();
});


add_test(function test_corruptFormHistoryDB_emptyInit() {
  do_log_info("test that FormHistory initializes an empty DB in place of corrupt DB.");

  FormHistory.count({}, {
    onSuccess : function(aNumEntries) {
      do_check_true(aNumEntries == 0);
      FormHistory.count({ fieldname : "name-A", value : "value-A" }, {
        onSuccess : function(aNumEntries2) {
          do_check_true(aNumEntries2 == 0);
          run_next_test();
        },
        onFailure : function(aError2) {
          do_throw("DB initialized after reading a corrupt DB file found an entry.");
        }
      });
    },
    onFailure : function (aError) {
      do_throw("DB initialized after reading a corrupt DB file is not empty.");
    }
  });
});

add_test(function test_corruptFormHistoryDB_addEntry() {
  do_log_info("test adding an entry to the empty DB.");

  FormHistory.update({
    op : "add",
    fieldname : "name-A",
    value : "value-A"
  }, {
    onSuccess : function() {
      FormHistory.count({ fieldname : "name-A", value : "value-A" }, {
        onSuccess : function(aNumEntries) {
          do_check_true(aNumEntries == 1);
          run_next_test();
        },
        onFailure : function(aError) {
          do_throw("Newly added entry cannot be found in DB.");
        }
      });
    },
    onFailure : function(aError) {
      do_throw("Unable to add new entry to empty DB.");
    }
  });
});

add_test(function test_corruptFormHistoryDB_removeEntry() {
  do_log_info("test removing an entry to the empty DB.");

  FormHistory.update({
    op : "remove",
    fieldname : "name-A",
    value : "value-A"
  }, {
    onSuccess : function() {
      FormHistory.count({ fieldname : "name-A", value : "value-A" }, {
        onSuccess : function(aNumEntries) {
          do_check_true(aNumEntries == 0);
          run_next_test();
        },
        onFailure : function(aError) {
          do_throw("Removed entry still being counted in DB.");
        }
      });
    },
    onFailure : function(aError) {
      do_throw("Unable to remove entry in DB.");
    }
  });
});
