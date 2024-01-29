/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let reportURL;

const { CrashReports } = ChromeUtils.importESModule(
  "resource://gre/modules/CrashReports.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  CrashSubmit: "resource://gre/modules/CrashSubmit.sys.mjs",
});

document.addEventListener("DOMContentLoaded", () => {
  populateReportLists();
  document
    .getElementById("clearUnsubmittedReports")
    .addEventListener("click", () => {
      clearUnsubmittedReports().catch(console.error);
    });
  document
    .getElementById("submitAllUnsubmittedReports")
    .addEventListener("click", () => {
      submitAllUnsubmittedReports().catch(console.error);
    });
  document
    .getElementById("clearSubmittedReports")
    .addEventListener("click", () => {
      clearSubmittedReports().catch(console.error);
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
  CrashSubmit.submit(reportId, CrashSubmit.SUBMITTED_FROM_ABOUT_CRASHES, {
    noThrottle: true,
  })
    .then(
      remoteCrashID => {
        document.getElementById("unsubmitted").removeChild(row);
        const report = CrashReports.getReports().filter(
          report => report.id === remoteCrashID
        );
        addReportRow(false, remoteCrashID, report.date, dateFormatter);
        showAppropriateSections();
        dispatchCustomEvent("CrashSubmitSucceeded");
      },
      () => {
        button.classList.remove("submitting");
        button.classList.add("failed-to-submit");
        document.l10n.setAttributes(
          buttonText,
          "submit-crash-button-failure-label"
        );
        dispatchCustomEvent("CrashSubmitFailed");
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

  await enqueueCleanup(() => cleanupFolder(CrashReports.pendingDir.path));
  await enqueueCleanup(clearOldReports);
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

  await enqueueCleanup(async () =>
    cleanupFolder(
      CrashReports.submittedDir.path,
      async entry => entry.name.startsWith("bp-") && entry.name.endsWith(".txt")
    )
  );
  await enqueueCleanup(clearOldReports);
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

    const stat = await IOUtils.stat(entry.path);
    return stat.lastModified < oneYearAgo;
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
  function entry(path) {
    return {
      path,
      name: PathUtils.filename(path),
    };
  }
  let children;
  try {
    children = await IOUtils.getChildren(path);
  } catch (e) {
    if (DOMException.isInstance(e) || e.name !== "NotFoundError") {
      throw e;
    }
  }

  for (const childPath of children) {
    if (!filter || (await filter(entry(childPath)))) {
      await IOUtils.remove(childPath);
    }
  }
}

/**
 * Dispatches an event with the specified name.
 *
 * @param {String} name the name of the event
 */
function dispatchCustomEvent(name) {
  document.dispatchEvent(
    new CustomEvent(name, { bubbles: true, cancelable: false })
  );
}

let cleanupQueue = Promise.resolve();

/**
 * Enqueue a cleanup function.
 *
 * Instead of directly calling cleanup functions as a result of DOM
 * interactions, queue them through this function so that we do not have
 * overlapping executions of cleanup functions.
 *
 * Cleanup functions overlapping could cause a race where one function is
 * attempting to stat a file while another function is attempting to delete it,
 * causing an exception.
 *
 * @param fn The cleanup function to call. It will be called once the last
 *           cleanup function has resolved.
 *
 * @returns A promise to await instead of awaiting the cleanup function.
 */
function enqueueCleanup(fn) {
  cleanupQueue = cleanupQueue.then(fn);
  return cleanupQueue;
}
