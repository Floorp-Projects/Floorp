/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Module to support exporting logins to a .csv file.
 */

const EXPORTED_SYMBOLS = ["LoginExport"];

let { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  OS: "resource://gre/modules/osfile.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

class LoginExport {
  /**
   * Builds an array of strings representing a row in a CSV.
   *
   * @param {nsILoginInfo} login
   *        The object that will be converted into a csv row.
   * @param {string[]} columns
   *        The CSV columns, used to find the properties from the login object.
   * @returns {string[]} Representing a row.
   */
  static _buildCSVRow(login, columns) {
    let row = [];
    for (let columnName of columns) {
      let columnValue = login[columnName];
      if (typeof columnValue == "string") {
        columnValue = columnValue.split('"').join('""');
      }
      if (columnValue !== null && columnValue != undefined) {
        row.push(`"${columnValue}"`);
      } else {
        row.push("");
      }
    }
    return row;
  }

  /**
   * Given a path it saves all the logins as a CSV file.
   *
   * @param {string} path
   *        The file path to save the login to.
   * @param {nsILoginInfo[]} [logins = null]
   *        An optional list of logins.
   */
  static async exportAsCSV(path, logins = null) {
    if (!logins) {
      logins = await Services.logins.getAllLoginsAsync();
    }
    let columns = [
      "origin",
      "username",
      "password",
      "httpRealm",
      "formActionOrigin",
      "guid",
      "timeCreated",
      "timeLastUsed",
      "timePasswordChanged",
    ];
    let csvHeader = columns.map(name => {
      if (name == "origin") {
        return '"url"';
      }
      return `"${name}"`;
    });

    let rows = [];
    rows.push(csvHeader);
    for (let login of logins) {
      rows.push(LoginExport._buildCSVRow(login, columns));
    }
    // https://tools.ietf.org/html/rfc7111 suggests always using CRLF.
    let csvAsString = rows.map(e => e.join(",")).join("\r\n");
    await OS.File.writeAtomic(path, new TextEncoder().encode(csvAsString), {
      tmpPath: path + ".tmp",
    });
  }
}
