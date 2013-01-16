/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

var reportsDir, submittedDir, pendingDir;
var reportURL;

Components.utils.import("resource://gre/modules/CrashSubmit.jsm");

function submitSuccess(dumpid, ret) {
  let link = document.getElementById(dumpid);
  if (link) {
    link.className = "";
    // reset the link to point at our new crash report. this way, if the
    // user clicks "Back", the link will be correct.
    let CrashID = ret.CrashID;
    link.firstChild.textContent = CrashID;
    link.setAttribute("id", CrashID);
    link.removeEventListener("click", submitPendingReport, true);

    if (reportURL) {
      link.setAttribute("href", reportURL + CrashID);
      // redirect the user to their brand new crash report
      window.location.href = reportURL + CrashID;
    }
  }
}

function submitError(dumpid) {
  //XXX: do something more useful here
  let link = document.getElementById(dumpid);
  if (link)
    link.className = "";
  // dispatch an event, useful for testing
  let event = document.createEvent("Events");
  event.initEvent("CrashSubmitFailed", true, false);
  document.dispatchEvent(event);
}

function submitPendingReport(event) {
  var link = event.target;
  var id = link.firstChild.textContent;
  if (CrashSubmit.submit(id, { submitSuccess: submitSuccess,
                               submitError: submitError,
                               noThrottle: true })) {
    link.className = "submitting";
  }
  event.preventDefault();
  return false;
}

function findInsertionPoint(reports, date) {
  if (reports.length == 0)
    return 0;

  var min = 0;
  var max = reports.length - 1;
  while (min < max) {
    var mid = parseInt((min + max) / 2);
    if (reports[mid].date < date)
      max = mid - 1;
    else if (reports[mid].date > date)
      min = mid + 1;
    else
      return mid;
  }
  if (reports[min].date <= date)
    return min;
  return min+1;
}

function populateReportList() {
  var prefService = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefBranch);

  try {
    reportURL = prefService.getCharPref("breakpad.reportURL");
    // Ignore any non http/https urls
    if (!/^https?:/i.test(reportURL))
      reportURL = null;
  }
  catch (e) { }
  if (!reportURL) {
    document.getElementById("clear-reports").style.display = "none";
    document.getElementById("reportList").style.display = "none";
    document.getElementById("noConfig").style.display = "block";
    return;
  }
  var directoryService = Cc["@mozilla.org/file/directory_service;1"].
                         getService(Ci.nsIProperties);

  reportsDir = directoryService.get("UAppData", Ci.nsIFile);
  reportsDir.append("Crash Reports");

  submittedDir = directoryService.get("UAppData", Ci.nsIFile);
  submittedDir.append("Crash Reports");
  submittedDir.append("submitted");

  var reports = [];
  if (submittedDir.exists() && submittedDir.isDirectory()) {
    var entries = submittedDir.directoryEntries;
    while (entries.hasMoreElements()) {
      var file = entries.getNext().QueryInterface(Ci.nsIFile);
      var leaf = file.leafName;
      if (leaf.substr(0, 3) == "bp-" &&
          leaf.substr(-4) == ".txt") {
        var entry = {
          id: leaf.slice(0, -4),
          date: file.lastModifiedTime,
          pending: false
        };
        var pos = findInsertionPoint(reports, entry.date);
        reports.splice(pos, 0, entry);
      }
    }
  }

  pendingDir = directoryService.get("UAppData", Ci.nsIFile);
  pendingDir.append("Crash Reports");
  pendingDir.append("pending");

  if (pendingDir.exists() && pendingDir.isDirectory()) {
    var entries = pendingDir.directoryEntries;
    while (entries.hasMoreElements()) {
      var file = entries.getNext().QueryInterface(Ci.nsIFile);
      var leaf = file.leafName;
      if (leaf.substr(-4) == ".dmp") {
        var entry = {
          id: leaf.slice(0, -4),
          date: file.lastModifiedTime,
          pending: true
        };
        var pos = findInsertionPoint(reports, entry.date);
        reports.splice(pos, 0, entry);
      }
    }
  }

  if (reports.length == 0) {
    document.getElementById("clear-reports").style.display = "none";
    document.getElementById("reportList").style.display = "none";
    document.getElementById("noReports").style.display = "block";
    return;
  }

  var formatter = Cc["@mozilla.org/intl/scriptabledateformat;1"].
                  createInstance(Ci.nsIScriptableDateFormat);
  var body = document.getElementById("tbody");
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var reportURI = ios.newURI(reportURL, null, null);
  // resolving this URI relative to /report/index
  var aboutThrottling = ios.newURI("../../about/throttling", null, reportURI);

  for (var i = 0; i < reports.length; i++) {
    var row = document.createElement("tr");
    var cell = document.createElement("td");
    row.appendChild(cell);
    var link = document.createElement("a");
    if (reports[i].pending) {
      link.setAttribute("href", aboutThrottling.spec);
      link.addEventListener("click", submitPendingReport, true);
    }
    else {
      link.setAttribute("href", reportURL + reports[i].id);
    }
    link.setAttribute("id", reports[i].id);
    link.appendChild(document.createTextNode(reports[i].id));
    cell.appendChild(link);

    var date = new Date(reports[i].date);
    cell = document.createElement("td");
    var datestr = formatter.FormatDate("",
                                       Ci.nsIScriptableDateFormat.dateFormatShort,
                                       date.getFullYear(),
                                       date.getMonth() + 1,
                                       date.getDate());
    cell.appendChild(document.createTextNode(datestr));
    row.appendChild(cell);
    cell = document.createElement("td");
    var timestr = formatter.FormatTime("",
                                       Ci.nsIScriptableDateFormat.timeFormatNoSeconds,
                                       date.getHours(),
                                       date.getMinutes(),
                                       date.getSeconds());
    cell.appendChild(document.createTextNode(timestr));
    row.appendChild(cell);
    body.appendChild(row);
  }
}

function clearReports() {
  var bundles = Cc["@mozilla.org/intl/stringbundle;1"].
                getService(Ci.nsIStringBundleService);
  var bundle = bundles.createBundle("chrome://global/locale/crashes.properties");
  var prompts = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                getService(Ci.nsIPromptService);
  if (!prompts.confirm(window,
                       bundle.GetStringFromName("deleteconfirm.title"),
                       bundle.GetStringFromName("deleteconfirm.description")))
    return;

  var entries = submittedDir.directoryEntries;
  while (entries.hasMoreElements()) {
    var file = entries.getNext().QueryInterface(Ci.nsIFile);
    var leaf = file.leafName;
    if (leaf.substr(0, 3) == "bp-" &&
        leaf.substr(-4) == ".txt") {
      file.remove(false);
    }
  }
  document.getElementById("clear-reports").style.display = "none";
  document.getElementById("reportList").style.display = "none";
  document.getElementById("noReports").style.display = "block";
}

function init() {
  populateReportList();
}
