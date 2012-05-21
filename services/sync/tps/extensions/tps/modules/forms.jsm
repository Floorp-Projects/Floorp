/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
   Components.utils.import() and acts as a singleton. Only the following
   listed symbols will exposed on import, and only when and where imported.
  */

var EXPORTED_SYMBOLS = ["FormData"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("resource://tps/logger.jsm");

let formService = CC["@mozilla.org/satchel/form-history;1"]
                  .getService(CI.nsIFormHistory2);

/**
 * FormDB
 *
 * Helper object containing methods to interact with the moz_formhistory
 * SQLite table.
 */
let FormDB = {
  /**
   * makeGUID
   *
   * Generates a brand-new globally unique identifier (GUID).  Borrowed
   * from Weave's utils.js.
   *
   * @return the new guid
   */
  makeGUID: function makeGUID() {
    // 70 characters that are not-escaped URL-friendly
    const code =
      "!()*-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~";

    let guid = "";
    let num = 0;
    let val;

    // Generate ten 70-value characters for a 70^10 (~61.29-bit) GUID
    for (let i = 0; i < 10; i++) {
      // Refresh the number source after using it a few times
      if (i == 0 || i == 5)
        num = Math.random();

      // Figure out which code to use for the next GUID character
      num *= 70;
      val = Math.floor(num);
      guid += code[val];
      num -= val;
    }

    return guid;
  },

  /**
   * insertValue
   *
   * Inserts the specified value for the specified fieldname into the
   * moz_formhistory table.
   *
   * @param fieldname The form fieldname to insert
   * @param value The form value to insert
   * @param us The time, in microseconds, to use for the lastUsed
   *        and firstUsed columns
   * @return nothing
   */
  insertValue: function (fieldname, value, us) {
    let query = this.createStatement(
      "INSERT INTO moz_formhistory " +
      "(fieldname, value, timesUsed, firstUsed, lastUsed, guid) VALUES " +
      "(:fieldname, :value, :timesUsed, :firstUsed, :lastUsed, :guid)");
    query.params.fieldname = fieldname;
    query.params.value = value;
    query.params.timesUsed = 1;
    query.params.firstUsed = us;
    query.params.lastUsed = us;
    query.params.guid = this.makeGUID();
    query.execute();
    query.reset();
  },

  /**
   * updateValue
   *
   * Updates a row in the moz_formhistory table with a new value.
   *
   * @param id The id of the row to update
   * @param newvalue The new value to set
   * @return nothing
   */
  updateValue: function (id, newvalue) {
    let query = this.createStatement(
      "UPDATE moz_formhistory SET value = :value WHERE id = :id");
    query.params.id = id;
    query.params.value = newvalue;
    query.execute();
    query.reset();
  },

  /**
   * getDataForValue
   *
   * Retrieves a set of values for a row in the database that
   * corresponds to the given fieldname and value.
   *
   * @param fieldname The fieldname of the row to query
   * @param value The value of the row to query
   * @return null if no row is found with the specified fieldname and value,
   *         or an object containing the row's id, lastUsed, and firstUsed
   *         values
   */
  getDataForValue: function (fieldname, value) {
    let query = this.createStatement(
      "SELECT id, lastUsed, firstUsed FROM moz_formhistory WHERE " +
      "fieldname = :fieldname AND value = :value");
    query.params.fieldname = fieldname;
    query.params.value = value;
    if (!query.executeStep())
      return null;

    return {
      id: query.row.id,
      lastUsed: query.row.lastUsed,
      firstUsed: query.row.firstUsed
    };
  },

  /**
   * createStatement
   *
   * Creates a statement from a SQL string.  This function is borrowed
   * from Weave's forms.js.
   *
   * @param query The SQL query string
   * @return the mozIStorageStatement created from the specified SQL
   */
  createStatement: function createStatement(query) {
    try {
      // Just return the statement right away if it's okay
      return formService.DBConnection.createStatement(query);
    }
    catch(ex) {
      // Assume guid column must not exist yet, so add it with an index
      formService.DBConnection.executeSimpleSQL(
        "ALTER TABLE moz_formhistory ADD COLUMN guid TEXT");
      formService.DBConnection.executeSimpleSQL(
        "CREATE INDEX IF NOT EXISTS moz_formhistory_guid_index " +
        "ON moz_formhistory (guid)");
    }

    // Try creating the query now that the column exists
    return formService.DBConnection.createStatement(query);
  }
};

/**
 * FormData class constructor
 *
 * Initializes instance properties.
 */
function FormData(props, usSinceEpoch) {
  this.fieldname = null;
  this.value = null;
  this.date = 0;
  this.newvalue = null;
  this.usSinceEpoch = usSinceEpoch;

  for (var prop in props) {
    if (prop in this)
      this[prop] = props[prop];
  }
}

/**
 * FormData instance methods
 */
FormData.prototype = {
  /**
   * hours_to_us
   *
   * Converts hours since present to microseconds since epoch.
   *
   * @param hours The number of hours since the present time (e.g., 0 is
   *        'now', and -1 is 1 hour ago)
   * @return the corresponding number of microseconds since the epoch
   */
  hours_to_us: function(hours) {
    return this.usSinceEpoch + (hours * 60 * 60 * 1000 * 1000);
  },

  /**
   * Create
   *
   * If this FormData object doesn't exist in the moz_formhistory database,
   * add it.  Throws on error.
   *
   * @return nothing
   */
  Create: function() {
    Logger.AssertTrue(this.fieldname != null && this.value != null,
      "Must specify both fieldname and value");

    let formdata = FormDB.getDataForValue(this.fieldname, this.value);
    if (!formdata) {
      // this item doesn't exist yet in the db, so we need to insert it
      FormDB.insertValue(this.fieldname, this.value,
                         this.hours_to_us(this.date));
    }
    else {
      /* Right now, we ignore this case.  If bug 552531 is ever fixed,
         we might need to add code here to update the firstUsed or
         lastUsed fields, as appropriate.
       */
    }
  },

  /**
   * Find
   *
   * Attempts to locate an entry in the moz_formhistory database that
   * matches the fieldname and value for this FormData object.
   *
   * @return true if this entry exists in the database, otherwise false
   */
  Find: function() {
    let formdata = FormDB.getDataForValue(this.fieldname, this.value);
    let status = formdata != null;
    if (status) {
      /*
      //form history dates currently not synced!  bug 552531
      let us = this.hours_to_us(this.date);
      status = Logger.AssertTrue(
        us >= formdata.firstUsed && us <= formdata.lastUsed,
        "No match for with that date value");

      if (status)
      */
        this.id = formdata.id;
    }
    return status;
  },

  /**
   * Remove
   *
   * Removes the row represented by this FormData instance from the
   * moz_formhistory database.
   *
   * @return nothing
   */
  Remove: function() {
    /* Right now Weave doesn't handle this correctly, see bug 568363.
     */
    formService.removeEntry(this.fieldname, this.value);
    return true;
  },
};
