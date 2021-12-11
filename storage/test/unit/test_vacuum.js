/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This file tests the Vacuum Manager.

/**
 * Loads a test component that will register as a vacuum-participant.
 * If other participants are found they will be unregistered, to avoid conflicts
 * with the test itself.
 */
function load_test_vacuum_component() {
  const CATEGORY_NAME = "vacuum-participant";

  do_load_manifest("vacuumParticipant.manifest");

  // This is a lazy check, there could be more participants than just this test
  // we just mind that the test exists though.
  const EXPECTED_ENTRIES = ["vacuumParticipant"];
  let { catMan } = Services;
  let found = false;
  for (let { data: entry } of catMan.enumerateCategory(CATEGORY_NAME)) {
    print("Check if the found category entry (" + entry + ") is expected.");
    if (EXPECTED_ENTRIES.includes(entry)) {
      print("Check that only one test entry exists.");
      Assert.ok(!found);
      found = true;
    } else {
      // Temporary unregister other participants for this test.
      catMan.deleteCategoryEntry("vacuum-participant", entry, false);
    }
  }
  print("Check the test entry exists.");
  Assert.ok(found);
}

/**
 * Sends a fake idle-daily notification to the VACUUM Manager.
 */
function synthesize_idle_daily() {
  let vm = Cc["@mozilla.org/storage/vacuum;1"].getService(Ci.nsIObserver);
  vm.observe(null, "idle-daily", null);
}

/**
 * Returns a new nsIFile reference for a profile database.
 * @param filename for the database, excluded the .sqlite extension.
 */
function new_db_file(name) {
  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append(name + ".sqlite");
  return file;
}

function run_test() {
  do_test_pending();

  // Change initial page size.  Do it immediately since it would require an
  // additional vacuum op to do it later.  As a bonus this makes the page size
  // change test really fast since it only has to check results.
  let conn = getDatabase(new_db_file("testVacuum"));
  conn.executeSimpleSQL("PRAGMA page_size = 1024");
  print("Check current page size.");
  let stmt = conn.createStatement("PRAGMA page_size");
  try {
    while (stmt.executeStep()) {
      Assert.equal(stmt.row.page_size, 1024);
    }
  } finally {
    stmt.finalize();
  }

  load_test_vacuum_component();

  run_next_test();
}

const TESTS = [
  function test_common_vacuum() {
    print(
      "\n*** Test that a VACUUM correctly happens and all notifications are fired."
    );
    // Wait for VACUUM begin.
    let beginVacuumReceived = false;
    Services.obs.addObserver(function onVacuum(aSubject, aTopic, aData) {
      Services.obs.removeObserver(onVacuum, aTopic);
      beginVacuumReceived = true;
    }, "test-begin-vacuum");

    // Wait for heavy IO notifications.
    let heavyIOTaskBeginReceived = false;
    let heavyIOTaskEndReceived = false;
    Services.obs.addObserver(function onVacuum(aSubject, aTopic, aData) {
      if (heavyIOTaskBeginReceived && heavyIOTaskEndReceived) {
        Services.obs.removeObserver(onVacuum, aTopic);
      }

      if (aData == "vacuum-begin") {
        heavyIOTaskBeginReceived = true;
      } else if (aData == "vacuum-end") {
        heavyIOTaskEndReceived = true;
      }
    }, "heavy-io-task");

    // Wait for VACUUM end.
    Services.obs.addObserver(function onVacuum(aSubject, aTopic, aData) {
      Services.obs.removeObserver(onVacuum, aTopic);
      print("Check we received onBeginVacuum");
      Assert.ok(beginVacuumReceived);
      print("Check we received heavy-io-task notifications");
      Assert.ok(heavyIOTaskBeginReceived);
      Assert.ok(heavyIOTaskEndReceived);
      print("Received onEndVacuum");
      run_next_test();
    }, "test-end-vacuum");

    synthesize_idle_daily();
  },

  function test_skipped_if_recent_vacuum() {
    print("\n*** Test that a VACUUM is skipped if it was run recently.");
    Services.prefs.setIntPref(
      "storage.vacuum.last.testVacuum.sqlite",
      parseInt(Date.now() / 1000)
    );

    // Wait for VACUUM begin.
    let vacuumObserver = {
      gotNotification: false,
      observe: function VO_observe(aSubject, aTopic, aData) {
        this.gotNotification = true;
      },
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    };
    Services.obs.addObserver(vacuumObserver, "test-begin-vacuum");

    // Check after a couple seconds that no VACUUM has been run.
    do_timeout(2000, function() {
      print("Check VACUUM did not run.");
      Assert.ok(!vacuumObserver.gotNotification);
      Services.obs.removeObserver(vacuumObserver, "test-begin-vacuum");
      run_next_test();
    });

    synthesize_idle_daily();
  },

  function test_page_size_change() {
    print("\n*** Test that a VACUUM changes page_size");

    // We did setup the database with a small page size, the previous vacuum
    // should have updated it.
    print("Check that page size was updated.");
    let conn = getDatabase(new_db_file("testVacuum"));
    let stmt = conn.createStatement("PRAGMA page_size");
    try {
      while (stmt.executeStep()) {
        Assert.equal(stmt.row.page_size, conn.defaultPageSize);
      }
    } finally {
      stmt.finalize();
    }

    run_next_test();
  },

  function test_skipped_optout_vacuum() {
    print(
      "\n*** Test that a VACUUM is skipped if the participant wants to opt-out."
    );
    Services.obs.notifyObservers(null, "test-options", "opt-out");

    // Wait for VACUUM begin.
    let vacuumObserver = {
      gotNotification: false,
      observe: function VO_observe(aSubject, aTopic, aData) {
        this.gotNotification = true;
      },
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    };
    Services.obs.addObserver(vacuumObserver, "test-begin-vacuum");

    // Check after a couple seconds that no VACUUM has been run.
    do_timeout(2000, function() {
      print("Check VACUUM did not run.");
      Assert.ok(!vacuumObserver.gotNotification);
      Services.obs.removeObserver(vacuumObserver, "test-begin-vacuum");
      run_next_test();
    });

    synthesize_idle_daily();
  },

  /* Changing page size on WAL is not supported till Bug 634374 is properly fixed.
  function test_page_size_change_with_wal()
  {
    print("\n*** Test that a VACUUM changes page_size with WAL mode");
    Services.obs.notifyObservers(null, "test-options", "wal");

    // Set a small page size.
    let conn = getDatabase(new_db_file("testVacuum2"));
    conn.executeSimpleSQL("PRAGMA page_size = 1024");
    let stmt = conn.createStatement("PRAGMA page_size");
    try {
      while (stmt.executeStep()) {
        do_check_eq(stmt.row.page_size, 1024);
      }
    }
    finally {
      stmt.finalize();
    }

    // Use WAL journal mode.
    conn.executeSimpleSQL("PRAGMA journal_mode = WAL");
    stmt = conn.createStatement("PRAGMA journal_mode");
    try {
      while (stmt.executeStep()) {
        do_check_eq(stmt.row.journal_mode, "wal");
      }
    }
    finally {
      stmt.finalize();
    }

    // Wait for VACUUM end.
    let vacuumObserver = {
      observe: function VO_observe(aSubject, aTopic, aData) {
        Services.obs.removeObserver(this, aTopic);
        print("Check page size has been updated.");
        let stmt = conn.createStatement("PRAGMA page_size");
        try {
          while (stmt.executeStep()) {
            do_check_eq(stmt.row.page_size, Ci.mozIStorageConnection.DEFAULT_PAGE_SIZE);
          }
        }
        finally {
          stmt.finalize();
        }

        print("Check journal mode has been restored.");
        stmt = conn.createStatement("PRAGMA journal_mode");
        try {
          while (stmt.executeStep()) {
            do_check_eq(stmt.row.journal_mode, "wal");
          }
        }
        finally {
          stmt.finalize();
        }

        run_next_test();
      },
      QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver])
    }
    Services.obs.addObserver(vacuumObserver, "test-end-vacuum", false);

    synthesize_idle_daily();
  },
  */

  function test_memory_database_crash() {
    print("\n*** Test that we don't crash trying to vacuum a memory database");
    Services.obs.notifyObservers(null, "test-options", "memory");

    // Wait for VACUUM begin.
    let vacuumObserver = {
      gotNotification: false,
      observe: function VO_observe(aSubject, aTopic, aData) {
        this.gotNotification = true;
      },
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    };
    Services.obs.addObserver(vacuumObserver, "test-begin-vacuum");

    // Check after a couple seconds that no VACUUM has been run.
    do_timeout(2000, function() {
      print("Check VACUUM did not run.");
      Assert.ok(!vacuumObserver.gotNotification);
      Services.obs.removeObserver(vacuumObserver, "test-begin-vacuum");
      run_next_test();
    });

    synthesize_idle_daily();
  },

  /* Changing page size on WAL is not supported till Bug 634374 is properly fixed.
  function test_wal_restore_fail()
  {
    print("\n*** Test that a failing WAL restoration notifies failure");
    Services.obs.notifyObservers(null, "test-options", "wal-fail");

    // Wait for VACUUM end.
    let vacuumObserver = {
      observe: function VO_observe(aSubject, aTopic, aData) {
        Services.obs.removeObserver(vacuumObserver, "test-end-vacuum");
        print("Check WAL restoration failed.");
        do_check_false(aData);
        run_next_test();
      },
      QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver])
    }
    Services.obs.addObserver(vacuumObserver, "test-end-vacuum", false);

    synthesize_idle_daily();
  },
  */
];

function run_next_test() {
  if (TESTS.length == 0) {
    Services.obs.notifyObservers(null, "test-options", "dispose");
    do_test_finished();
  } else {
    // Set last VACUUM to a date in the past.
    Services.prefs.setIntPref(
      "storage.vacuum.last.testVacuum.sqlite",
      parseInt(Date.now() / 1000 - 31 * 86400)
    );
    executeSoon(TESTS.shift());
  }
}
