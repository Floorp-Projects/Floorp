/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides a class to import login-related data CSV files.
 */

"use strict";

const EXPORTED_SYMBOLS = ["LoginCSVImport"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  ResponsivenessMonitor: "resource://gre/modules/ResponsivenessMonitor.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "d3", () => {
  let d3Scope = Cu.Sandbox(null);
  Services.scriptloader.loadSubScript(
    "chrome://global/content/third_party/d3/d3.js",
    d3Scope
  );
  return Cu.waiveXrays(d3Scope.d3);
});

const FIELD_TO_CSV_COLUMNS = {
  origin: ["url", "login_uri"],
  username: ["username", "login_username"],
  password: ["password", "login_password"],
  httpRealm: ["httpRealm"],
  formActionOrigin: ["formActionOrigin"],
  guid: ["guid"],
  timeCreated: ["timeCreated"],
  timeLastUsed: ["timeLastUsed"],
  timePasswordChanged: ["timePasswordChanged"],
};

/**
 * Provides an object that has a method to import login-related data CSV files
 */
class LoginCSVImport {
  static get MIGRATION_HISTOGRAM_KEY() {
    return "login_csv";
  }

  /**
   * Returns a map that has the csv column name as key and the value the field name.
   *
   * @returns {Map} A map that has the csv column name as key and the value the field name.
   */
  static _getCSVColumnToFieldMap() {
    let csvColumnToField = new Map();
    for (let [field, columns] of Object.entries(FIELD_TO_CSV_COLUMNS)) {
      for (let column of columns) {
        csvColumnToField.set(column, field);
      }
    }
    return csvColumnToField;
  }

  /**
   * Builds a vanilla JS object containing all the login fields from a row of CSV cells.
   *
   * @param {object} csvObject
   *        An object created from a csv row. The keys are the csv column names, the values are the cells.
   * @param {Map} csvColumnToFieldMap
   *        A map where the keys are the csv properties and the values are the object keys.
   * @returns {object} Representing login object with only properties, not functions.
   */
  static _getVanillaLoginFromCSVObject(csvObject, csvColumnToFieldMap) {
    let vanillaLogin = Object.create(null);
    for (let columnName of Object.keys(csvObject)) {
      let fieldName = csvColumnToFieldMap.get(columnName);
      if (!fieldName) {
        continue;
      }

      if (
        typeof vanillaLogin[fieldName] != "undefined" &&
        vanillaLogin[fieldName] !== csvObject[columnName]
      ) {
        // Differing column values map to one property.
        // e.g. if two headings map to `origin` we won't know which to use.
        return {};
      }

      vanillaLogin[fieldName] = csvObject[columnName];
    }

    // Since `null` can't be represented in a CSV file and the httpRealm header
    // cannot be an empty string, assume that an empty httpRealm means this is
    // a form login and therefore null-out httpRealm.
    if (vanillaLogin.httpRealm === "") {
      vanillaLogin.httpRealm = null;
    }

    return vanillaLogin;
  }

  /**
   * Imports logins from a CSV file (comma-separated values file).
   * Existing logins may be updated in the process.
   *
   * @param {string} filePath
   */
  static async importFromCSV(filePath) {
    TelemetryStopwatch.startKeyed(
      "FX_MIGRATION_LOGINS_IMPORT_MS",
      LoginCSVImport.MIGRATION_HISTOGRAM_KEY
    );
    let responsivenessMonitor = new ResponsivenessMonitor();
    let csvColumnToFieldMap = LoginCSVImport._getCSVColumnToFieldMap();
    let csvString = await OS.File.read(filePath, { encoding: "utf-8" });
    let parsedLines = d3.csv.parse(csvString);
    let fieldsInFile = new Set(
      Object.keys(parsedLines[0] || {}).map(col => csvColumnToFieldMap.get(col))
    );
    if (
      parsedLines[0] &&
      (!fieldsInFile.has("origin") ||
        !fieldsInFile.has("username") ||
        !fieldsInFile.has("password"))
    ) {
      // The username *value* can be empty but we require a username column to
      // ensure that we don't import logins without their usernames due to the
      // username column not being recognized.
      TelemetryStopwatch.cancelKeyed(
        "FX_MIGRATION_LOGINS_IMPORT_MS",
        LoginCSVImport.MIGRATION_HISTOGRAM_KEY
      );
      throw new Error(
        "CSV file must contain origin, username, and password columns"
      );
    }

    let loginsToImport = parsedLines.map(csvObject => {
      return LoginCSVImport._getVanillaLoginFromCSVObject(
        csvObject,
        csvColumnToFieldMap
      );
    });

    await LoginHelper.maybeImportLogins(loginsToImport);

    // Record quantity, jank, and duration telemetry.
    try {
      Services.telemetry
        .getKeyedHistogramById("FX_MIGRATION_LOGINS_QUANTITY")
        .add(LoginCSVImport.MIGRATION_HISTOGRAM_KEY, parsedLines.length);
      let accumulatedDelay = responsivenessMonitor.finish();
      Services.telemetry
        .getKeyedHistogramById("FX_MIGRATION_LOGINS_JANK_MS")
        .add(LoginCSVImport.MIGRATION_HISTOGRAM_KEY, accumulatedDelay);
      TelemetryStopwatch.finishKeyed(
        "FX_MIGRATION_LOGINS_IMPORT_MS",
        LoginCSVImport.MIGRATION_HISTOGRAM_KEY
      );
    } catch (ex) {
      Cu.reportError(ex);
    }
  }
}
