/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This testing component is used in test_vacuum* tests.

var EXPORTED_SYMBOLS = ["VacuumParticipant"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * Returns a new nsIFile reference for a profile database.
 * @param filename for the database, excluded the .sqlite extension.
 */
function new_db_file(name) {
  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append(name + ".sqlite");
  return file;
}

/**
 * Opens and returns a connection to the provided database file.
 * @param nsIFile interface to the database file.
 */
function getDatabase(aFile) {
  return Services.storage.openDatabase(aFile);
}

function VacuumParticipant() {
  this._dbConn = getDatabase(new_db_file("testVacuum"));
  Services.obs.addObserver(this, "test-options");
}

VacuumParticipant.prototype = {
  get expectedDatabasePageSize() {
    return this._dbConn.defaultPageSize;
  },
  get databaseConnection() {
    return this._dbConn;
  },

  _grant: true,
  onBeginVacuum: function TVP_onBeginVacuum() {
    if (!this._grant) {
      this._grant = true;
      return false;
    }
    Services.obs.notifyObservers(null, "test-begin-vacuum");
    return true;
  },
  onEndVacuum: function TVP_EndVacuum(aSucceeded) {
    if (this._stmt) {
      this._stmt.finalize();
    }
    Services.obs.notifyObservers(null, "test-end-vacuum", aSucceeded);
  },

  observe: function TVP_observe(aSubject, aTopic, aData) {
    if (aData == "opt-out") {
      this._grant = false;
    } else if (aData == "wal") {
      try {
        this._dbConn.close();
      } catch (e) {
        // Do nothing.
      }
      this._dbConn = getDatabase(new_db_file("testVacuum2"));
    } else if (aData == "wal-fail") {
      try {
        this._dbConn.close();
      } catch (e) {
        // Do nothing.
      }
      this._dbConn = getDatabase(new_db_file("testVacuum3"));
      // Use WAL journal mode.
      this._dbConn.executeSimpleSQL("PRAGMA journal_mode = WAL");
      // Create a not finalized statement.
      this._stmt = this._dbConn.createStatement("SELECT :test");
      this._stmt.params.test = 1;
      this._stmt.executeStep();
    } else if (aData == "memory") {
      try {
        this._dbConn.asyncClose();
      } catch (e) {
        // Do nothing.
      }
      this._dbConn = Services.storage.openSpecialDatabase("memory");
    } else if (aData == "dispose") {
      Services.obs.removeObserver(this, "test-options");
      try {
        this._dbConn.asyncClose();
      } catch (e) {
        // Do nothing.
      }
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "mozIStorageVacuumParticipant",
    "nsIObserver",
  ]),
};
