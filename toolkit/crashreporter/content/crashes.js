/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, utils: Cu, interfaces: Ci } = Components;

var reportURL;

Cu.import("resource://gre/modules/CrashReports.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CrashSubmit",
  "resource://gre/modules/CrashSubmit.jsm");

const buildID = Services.appinfo.appBuildID;

function submitPendingReport(event) {
  let link = event.target;
  let id = link.firstChild.textContent;
  link.className = "submitting";
  CrashSubmit.submit(id, { noThrottle: true }).then(
    (remoteCrashID) => {
      link.className = "";
      // Reset the link to point at our new crash report. This way, if the
      // user clicks "Back", the link will be correct.
      link.firstChild.textContent = remoteCrashID;
      link.setAttribute("id", remoteCrashID);
      link.removeEventListener("click", submitPendingReport, true);

      if (reportURL) {
        link.setAttribute("href", reportURL + remoteCrashID);
        // redirect the user to their brand new crash report
        window.location.href = reportURL + remoteCrashID;
      }
    },
    () => {
      // XXX: do something more useful here
      link.className = "";

      // Dispatch an event, useful for testing
      let event = document.createEvent("Events");
      event.initEvent("CrashSubmitFailed", true, false);
      document.dispatchEvent(event);
    });
  event.preventDefault();
  return false;
}

function populateReportList() {

  var prefService = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefBranch);

  try {
    reportURL = prefService.getCharPref("breakpad.reportURL");
    // Ignore any non http/https urls
    if (!/^https?:/i.test(reportURL))
      reportURL = null;
  } catch (e) { }
  if (!reportURL) {
    document.getElementById("clear-reports").style.display = "none";
    document.getElementById("reportList").style.display = "none";
    document.getElementById("noConfig").style.display = "block";
    return;
  }
  let reports = CrashReports.getReports();

  if (reports.length == 0) {
    document.getElementById("clear-reports").style.display = "none";
    document.getElementById("reportList").style.display = "none";
    document.getElementById("noReports").style.display = "block";
    return;
  }

  var dateFormatter;
  var timeFormatter;
  try {
    dateFormatter = Services.intl.createDateTimeFormat(undefined, { dateStyle: "short" });
    timeFormatter = Services.intl.createDateTimeFormat(undefined, { timeStyle: "short" });
  } catch (e) {
    // XXX Fallback to be removed once bug 1215247 is complete
    // and the Intl API is available on all platforms.
    dateFormatter = {
      format(date) {
        return date.toLocaleDateString();
      }
    };
    timeFormatter = {
      format(date) {
        return date.toLocaleTimeString();
      }
    };
  }
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var reportURI = ios.newURI(reportURL);
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
    } else {
      link.setAttribute("href", reportURL + reports[i].id);
    }
    link.setAttribute("id", reports[i].id);
    link.classList.add("crashReport");
    link.appendChild(document.createTextNode(reports[i].id));
    cell.appendChild(link);

    var date = new Date(reports[i].date);
    cell = document.createElement("td");
    cell.appendChild(document.createTextNode(dateFormatter.format(date)));
    row.appendChild(cell);
    cell = document.createElement("td");
    cell.appendChild(document.createTextNode(timeFormatter.format(date)));
    row.appendChild(cell);
    if (reports[i].pending) {
      document.getElementById("unsubmitted").appendChild(row);
    } else {
      document.getElementById("submitted").appendChild(row);
    }
  }
}

var clearReports = async function() {
  let bundle = Services.strings.createBundle("chrome://global/locale/crashes.properties");

  if (!Services.
         prompt.confirm(window,
                        bundle.GetStringFromName("deleteconfirm.title"),
                        bundle.GetStringFromName("deleteconfirm.description"))) {
    return;
  }

  let cleanupFolder = async function(path, filter) {
    let iterator = new OS.File.DirectoryIterator(path);
    try {
      await iterator.forEach(async function(aEntry) {
        if (!filter || (await filter(aEntry))) {
          await OS.File.remove(aEntry.path);
        }
      });
    } catch (e) {
      if (!(e instanceof OS.File.Error) || !e.becauseNoSuchFile) {
        throw e;
      }
    } finally {
      iterator.close();
    }
  };

  await cleanupFolder(CrashReports.submittedDir.path, function(aEntry) {
    return aEntry.name.startsWith("bp-") && aEntry.name.endsWith(".txt");
  });

  let oneYearAgo = Date.now() - 31586000000;
  await cleanupFolder(CrashReports.reportsDir.path, async function(aEntry) {
    if (!aEntry.name.startsWith("InstallTime") ||
        aEntry.name == "InstallTime" + buildID) {
      return false;
    }

    let date = aEntry.winLastWriteDate;
    if (!date) {
      let stat = await OS.File.stat(aEntry.path);
      date = stat.lastModificationDate;
    }

    return (date < oneYearAgo);
  });

  await cleanupFolder(CrashReports.pendingDir.path);

  document.getElementById("clear-reports").style.display = "none";
  document.getElementById("reportList").style.display = "none";
  document.getElementById("noReports").style.display = "block";
};
