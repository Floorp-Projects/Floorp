/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let reportURL;

const { CrashReports } = ChromeUtils.import(
  "resource://gre/modules/CrashReports.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "CrashSubmit",
  "resource://gre/modules/CrashSubmit.jsm"
);

document.addEventListener("DOMContentLoaded", () => {
  populateReportLists();
  document
    .getElementById("clearUnsubmittedReports")
    .addEventListener("click", () => {
      clearUnsubmittedReports().catch(Cu.reportError);
    });
  document
    .getElementById("submitAllUnsubmittedReports")
    .addEventListener("click", () => {
      submitAllUnsubmittedReports().catch(Cu.reportError);
    });
  document
    .getElementById("clearSubmittedReports")
    .addEventListener("click", () => {
      clearSubmittedReports().catch(Cu.reportError);
    });
});

const buildID = Services.appinfo.appBuildID;

/**
 * Adds the crash reports with submission buttons and links
 * to the unsubmitted and submitted crash report lists.
 * If breakpad.reportURL is not set, displays a misconfiguration message
 * instead.
 */
function populateReportLists() {
  try {
    reportURL = Services.prefs.getCharPref("breakpad.reportURL");
    // Ignore any non http/https urls
    if (!/^https?:/i.test(reportURL)) {
      reportURL = null;
    }
  } catch (e) {
    reportURL = null;
  }
  if (!reportURL) {
    document.getElementById("noConfig").classList.remove("hidden");
    return;
  }

  const reports = CrashReports.getReports();
  const dateFormatter = new Services.intl.DateTimeFormat(undefined, {
    timeStyle: "short",
    dateStyle: "short",
  });
  reports.forEach(report =>
    addReportRow(report.pending, report.id, report.date, dateFormatter)
  );
  showAppropriateSections();
}

/**
 * Adds a crash report with the appropriate submission button
 * or viewing link to the unsubmitted or submitted report list
 * based on isPending.
 *
 * @param {Boolean} isPending     whether the crash is up for submission
 * @param {String}  id            the unique id of the crash report
 * @param {Date}    date          either the date of crash or date of submission
 * @param {Object}  dateFormatter formatter for presenting dates to users
 */
function addReportRow(isPending, id, date, dateFormatter) {
  const rowTemplate = document.getElementById("crashReportRow");
  const row = document
    .importNode(rowTemplate.content, true)
    .querySelector("tr");
  row.id = id;

  const cells = row.querySelectorAll("td");
  cells[0].appendChild(document.createTextNode(id));
  cells[1].appendChild(document.createTextNode(dateFormatter.format(date)));

  if (isPending) {
    const buttonTemplate = document.getElementById("crashSubmitButton");
    const button = document
      .importNode(buttonTemplate.content, true)
      .querySelector("button");
    const buttonText = button.querySelector("span");
    button.addEventListener("click", () =>
      submitPendingReport(id, row, button, buttonText, dateFormatter)
    );
    cells[2].appendChild(button);
    document.getElementById("unsubmitted").appendChild(row);
  } else {
    const linkTemplate = document.getElementById("viewCrashLink");
    const link = document
      .importNode(linkTemplate.content, true)
      .querySelector("a");
    link.href = `${reportURL}${id}`;
    cells[2].appendChild(link);
    document.getElementById("submitted").appendChild(row);
  }
}

/**
 * Shows or hides each of the unsubmitted and submitted report list
 * based on whether they contain at least one crash report.
 * If hidden, the submitted report list is replaced by a message
 * indicating that no crash reports have been submitted.
 */
function showAppropriateSections() {
  let hasUnsubmitted =
    document.getElementById("unsubmitted").childElementCount > 0;
  document
    .getElementById("reportListUnsubmitted")
    .classList.toggle("hidden", !hasUnsubmitted);

  let hasSubmitted = document.getElementById("submitted").childElementCount > 0;
  document
    .getElementById("reportListSubmitted")
    .classList.toggle("hidden", !hasSubmitted);
  document
    .getElementById("noSubmittedReports")
    .classList.toggle("hidden", hasSubmitted);
}

/**
 * Changes the provided button to display a spinner. Then, tries to submit the
 * crash report for the provided id. On success, removes the crash report from
 * the list of unsubmitted crash reports and adds a new crash report to the list
 * of submitted crash reports. On failure, changes the provided button to display
 * a red error message.
 *
 * @param {String}              reportId      the unique id of the crash report
 * @param {HTMLTableRowElement} row           the table row of the crash report
 * @param {HTMLButtonElement}   button        the button pressed to start the submission
 * @param {HTMLSpanElement}     buttonText    the text inside the pressed button
 * @param {Object}              dateFormatter formatter for presenting dates to users
 */
function submitPendingReport(reportId, row, button, buttonText, dateFormatter) {
  button.classList.add("submitting");
  document.getElementById("submitAllUnsubmittedReports").disabled = true;
  CrashSubmit.submit(reportId, { noThrottle: true })
    .then(
      remoteCrashID => {
        document.getElementById("unsubmitted").removeChild(row);
        const report = CrashReports.getReports().filter(
          report => report.id === remoteCrashID
        );
        addReportRow(false, remoteCrashID, report.date, dateFormatter);
        showAppropriateSections();
        dispatchEvent("CrashSubmitSucceeded");
      },
      () => {
        button.classList.remove("submitting");
        button.classList.add("failed-to-submit");
        document.l10n.setAttributes(
          buttonText,
          "submit-crash-button-failure-label"
        );
        dispatchEvent("CrashSubmitFailed");
      }
    )
    .finally(() => {
      document.getElementById("submitAllUnsubmittedReports").disabled = false;
    });
}

/**
 * Deletes unsubmitted and old crash reports from the user's device.
 * Then, hides the list of unsubmitted crash reports.
 */
async function clearUnsubmittedReports() {
  const [title, description] = await document.l10n.formatValues([
    { id: "delete-confirm-title" },
    { id: "delete-unsubmitted-description" },
  ]);
  if (!Services.prompt.confirm(window, title, description)) {
    return;
  }

  await cleanupFolder(CrashReports.pendingDir.path);
  await clearOldReports();
  document.getElementById("reportListUnsubmitted").classList.add("hidden");
}

/**
 * Submits all the pending crash reports and removes all pending reports from pending reports list
 * and add them to submitted crash reports.
 */
async function submitAllUnsubmittedReports() {
  for (
    var i = 0;
    i < document.getElementById("unsubmitted").childNodes.length;
    i++
  ) {
    document
      .getElementById("unsubmitted")
      .childNodes[i].cells[2].childNodes[0].click();
  }
}

/**
 * Deletes submitted and old crash reports from the user's device.
 * Then, hides the list of submitted crash reports.
 */
async function clearSubmittedReports() {
  const [title, description] = await document.l10n.formatValues([
    { id: "delete-confirm-title" },
    { id: "delete-submitted-description" },
  ]);
  if (!Services.prompt.confirm(window, title, description)) {
    return;
  }

  await cleanupFolder(
    CrashReports.submittedDir.path,
    async entry => entry.name.startsWith("bp-") && entry.name.endsWith(".txt")
  );
  await clearOldReports();
  document.getElementById("reportListSubmitted").classList.add("hidden");
  document.getElementById("noSubmittedReports").classList.remove("hidden");
}

/**
 * Deletes old crash reports from the user's device.
 */
async function clearOldReports() {
  const oneYearAgo = Date.now() - 31586000000;
  await cleanupFolder(CrashReports.reportsDir.path, async entry => {
    if (
      !entry.name.startsWith("InstallTime") ||
      entry.name == "InstallTime" + buildID
    ) {
      return false;
    }

    let date = entry.winLastWriteDate;
    if (!date) {
      const stat = await OS.File.stat(entry.path);
      date = stat.lastModificationDate;
    }
    return date < oneYearAgo;
  });
}

/**
 * Deletes files from the user's device at the specified path
 * that match the provided filter.
 *
 * @param {String}   path   the directory location to delete form
 * @param {Function} filter function taking in a file entry and
 *                          returning whether to delete the file
 */
async function cleanupFolder(path, filter) {
  const iterator = new OS.File.DirectoryIterator(path);
  try {
    await iterator.forEach(async entry => {
      if (!filter || (await filter(entry))) {
        await OS.File.remove(entry.path);
      }
    });
  } catch (e) {
    if (!(e instanceof OS.File.Error) || !e.becauseNoSuchFile) {
      throw e;
    }
  } finally {
    iterator.close();
  }
}

/**
 * Dispatches an event with the specified name.
 *
 * @param {String} name the name of the event
 */
function dispatchEvent(name) {
  const event = document.createEvent("Events");
  event.initEvent(name, true, false);
  document.dispatchEvent(event);
}
