/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = [
  "CrashReports"
];

this.CrashReports = {
  pendingDir: null,
  reportsDir: null,
  submittedDir: null,
  getReports: function CrashReports_getReports() {
    let reports = [];

    try {
      // Ignore any non http/https urls
      if (!/^https?:/i.test(Services.prefs.getCharPref("breakpad.reportURL")))
        return reports;
    } catch (e) { }

    if (this.submittedDir.exists() && this.submittedDir.isDirectory()) {
      let entries = this.submittedDir.directoryEntries;
      while (entries.hasMoreElements()) {
        let file = entries.getNext().QueryInterface(Components.interfaces.nsIFile);
        let leaf = file.leafName;
        if (leaf.startsWith("bp-") &&
            leaf.endsWith(".txt")) {
          let entry = {
            id: leaf.slice(0, -4),
            date: file.lastModifiedTime,
            pending: false
          };
          reports.push(entry);
        }
      }
    }

    if (this.pendingDir.exists() && this.pendingDir.isDirectory()) {
      let uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
      let entries = this.pendingDir.directoryEntries;
      while (entries.hasMoreElements()) {
        let file = entries.getNext().QueryInterface(Components.interfaces.nsIFile);
        let leaf = file.leafName;
        let id = leaf.slice(0, -4);
        if (leaf.endsWith(".dmp") && uuidRegex.test(id)) {
          let entry = {
            id,
            date: file.lastModifiedTime,
            pending: true
          };
          reports.push(entry);
        }
      }
    }

    // Sort reports descending by date
    return reports.sort( (a, b) => b.date - a.date);
  }
};

function CrashReports_pendingDir() {
  let pendingDir = Services.dirsvc.get("UAppData", Components.interfaces.nsIFile);
  pendingDir.append("Crash Reports");
  pendingDir.append("pending");
  return pendingDir;
}

function CrashReports_reportsDir() {
  let reportsDir = Services.dirsvc.get("UAppData", Components.interfaces.nsIFile);
  reportsDir.append("Crash Reports");
  return reportsDir;
}

function CrashReports_submittedDir() {
  let submittedDir = Services.dirsvc.get("UAppData", Components.interfaces.nsIFile);
  submittedDir.append("Crash Reports");
  submittedDir.append("submitted");
  return submittedDir;
}

this.CrashReports.pendingDir = CrashReports_pendingDir();
this.CrashReports.reportsDir = CrashReports_reportsDir();
this.CrashReports.submittedDir = CrashReports_submittedDir();
