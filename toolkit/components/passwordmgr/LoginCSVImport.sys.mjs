/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides a class to import login-related data CSV files.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CSV: "resource://gre/modules/CSV.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
});

/**
 * All the CSV column names will be converted to lower case before lookup
 * so they must be specified here in lower case.
 */
const FIELD_TO_CSV_COLUMNS = {
  origin: ["url", "login_uri"],
  username: ["username", "login_username"],
  password: ["password", "login_password"],
  httpRealm: ["httprealm"],
  formActionOrigin: ["formactionorigin"],
  guid: ["guid"],
  timeCreated: ["timecreated"],
  timeLastUsed: ["timelastused"],
  timePasswordChanged: ["timepasswordchanged"],
};

export const ImportFailedErrorType = Object.freeze({
  CONFLICTING_VALUES_ERROR: "CONFLICTING_VALUES_ERROR",
  FILE_FORMAT_ERROR: "FILE_FORMAT_ERROR",
  FILE_PERMISSIONS_ERROR: "FILE_PERMISSIONS_ERROR",
  UNABLE_TO_READ_ERROR: "UNABLE_TO_READ_ERROR",
});

export class ImportFailedException extends Error {
  constructor(errorType, message) {
    super(message != null ? message : errorType);
    this.errorType = errorType;
  }
}

/**
 * Provides an object that has a method to import login-related data CSV files
 */
export class LoginCSVImport {
  /**
   * Returns a map that has the csv column name as key and the value the field name.
   *
   * @returns {Map} A map that has the csv column name as key and the value the field name.
   */
  static _getCSVColumnToFieldMap() {
    let csvColumnToField = new Map();
    for (let [field, columns] of Object.entries(FIELD_TO_CSV_COLUMNS)) {
      for (let column of columns) {
        csvColumnToField.set(column.toLowerCase(), field);
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
      let fieldName = csvColumnToFieldMap.get(columnName.toLowerCase());
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
  static _recordHistogramTelemetry(histogram, report) {
    for (let reportRow of report) {
      let { result } = reportRow;
      if (result.includes("error")) {
        histogram.add("error");
      } else {
        histogram.add(result);
      }
    }
  }
  /**
   * Imports logins from a CSV file (comma-separated values file).
   * Existing logins may be updated in the process.
   *
   * @param {string} filePath
   * @returns {Object[]} An array of rows where each is mapped to a row in the CSV and it's import information.
   */
  static async importFromCSV(filePath) {
    let csvColumnToFieldMap = LoginCSVImport._getCSVColumnToFieldMap();
    let csvFieldToColumnMap = new Map();

    let csvString;
    try {
      csvString = await IOUtils.readUTF8(filePath, { encoding: "utf-8" });
    } catch (ex) {
      console.error(ex);
      throw new ImportFailedException(
        ImportFailedErrorType.FILE_PERMISSIONS_ERROR
      );
    }
    let headerLine;
    let parsedLines;
    try {
      let delimiter = filePath.toUpperCase().endsWith(".CSV") ? "," : "\t";
      [headerLine, parsedLines] = lazy.CSV.parse(csvString, delimiter);
    } catch {
      throw new ImportFailedException(ImportFailedErrorType.FILE_FORMAT_ERROR);
    }
    if (parsedLines && headerLine) {
      for (const columnName of headerLine) {
        const fieldName = csvColumnToFieldMap.get(
          columnName.toLocaleLowerCase()
        );
        if (fieldName) {
          if (!csvFieldToColumnMap.has(fieldName)) {
            csvFieldToColumnMap.set(fieldName, columnName);
          } else {
            throw new ImportFailedException(
              ImportFailedErrorType.CONFLICTING_VALUES_ERROR
            );
          }
        }
      }
    }
    if (csvFieldToColumnMap.size === 0) {
      throw new ImportFailedException(ImportFailedErrorType.FILE_FORMAT_ERROR);
    }
    if (
      parsedLines[0] &&
      (!csvFieldToColumnMap.has("origin") ||
        !csvFieldToColumnMap.has("username") ||
        !csvFieldToColumnMap.has("password"))
    ) {
      // The username *value* can be empty but we require a username column to
      // ensure that we don't import logins without their usernames due to the
      // username column not being recognized.
      throw new ImportFailedException(ImportFailedErrorType.FILE_FORMAT_ERROR);
    }

    let loginsToImport = parsedLines.map(csvObject => {
      return LoginCSVImport._getVanillaLoginFromCSVObject(
        csvObject,
        csvColumnToFieldMap
      );
    });

    let report = await lazy.LoginHelper.maybeImportLogins(loginsToImport);

    for (const reportRow of report) {
      if (reportRow.result === "error_missing_field") {
        reportRow.field_name = csvFieldToColumnMap.get(reportRow.field_name);
      }
    }

    // Record quantity and duration telemetry.
    try {
      let histogram = Services.telemetry.getHistogramById(
        "PWMGR_IMPORT_LOGINS_FROM_FILE_CATEGORICAL"
      );
      this._recordHistogramTelemetry(histogram, report);
    } catch (ex) {
      console.error(ex);
    }
    LoginCSVImport.lastImportReport = report;
    return report;
  }
}
