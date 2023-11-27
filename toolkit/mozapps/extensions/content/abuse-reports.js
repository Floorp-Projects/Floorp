/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint max-len: ["error", 80] */
/* import-globals-from aboutaddonsCommon.js */
/* exported openAbuseReport */
/* global windowRoot */

/**
 * This script is part of the HTML about:addons page and it provides some
 * helpers used for the Abuse Reporting submission (and related message bars).
 */

const { AbuseReporter } = ChromeUtils.importESModule(
  "resource://gre/modules/AbuseReporter.sys.mjs"
);

// Message Bars definitions.
const ABUSE_REPORT_MESSAGE_BARS = {
  // Idle message-bar (used while the submission is still ongoing).
  submitting: {
    actions: ["cancel"],
    l10n: {
      id: "abuse-report-messagebar-submitting2",
      actionIds: {
        cancel: "abuse-report-messagebar-action-cancel",
      },
    },
  },
  // Submitted report message-bar.
  submitted: {
    actions: ["remove", "keep"],
    dismissable: true,
    l10n: {
      id: "abuse-report-messagebar-submitted2",
      actionIdsPerAddonType: {
        extension: {
          remove: "abuse-report-messagebar-action-remove-extension",
          keep: "abuse-report-messagebar-action-keep-extension",
        },
        sitepermission: {
          remove: "abuse-report-messagebar-action-remove-sitepermission",
          keep: "abuse-report-messagebar-action-keep-sitepermission",
        },
        theme: {
          remove: "abuse-report-messagebar-action-remove-theme",
          keep: "abuse-report-messagebar-action-keep-theme",
        },
      },
    },
  },
  // Submitted report message-bar (with no remove actions).
  "submitted-no-remove-action": {
    dismissable: true,
    l10n: { id: "abuse-report-messagebar-submitted-noremove2" },
  },
  // Submitted report and remove addon message-bar.
  "submitted-and-removed": {
    dismissable: true,
    l10n: {
      idsPerAddonType: {
        extension: "abuse-report-messagebar-removed-extension2",
        sitepermission: "abuse-report-messagebar-removed-sitepermission2",
        theme: "abuse-report-messagebar-removed-theme2",
      },
    },
  },
  // The "aborted report" message bar is rendered as a generic informative one,
  // because aborting a report is triggered by a user choice.
  ERROR_ABORTED_SUBMIT: {
    type: "info",
    dismissable: true,
    l10n: { id: "abuse-report-messagebar-aborted2" },
  },
  // Errors message bars.
  ERROR_ADDON_NOTFOUND: {
    type: "error",
    dismissable: true,
    l10n: { id: "abuse-report-messagebar-error2" },
  },
  ERROR_CLIENT: {
    type: "error",
    dismissable: true,
    l10n: { id: "abuse-report-messagebar-error2" },
  },
  ERROR_NETWORK: {
    actions: ["retry", "cancel"],
    type: "error",
    l10n: {
      id: "abuse-report-messagebar-error2",
      actionIds: {
        retry: "abuse-report-messagebar-action-retry",
        cancel: "abuse-report-messagebar-action-cancel",
      },
    },
  },
  ERROR_RECENT_SUBMIT: {
    actions: ["retry", "cancel"],
    type: "error",
    l10n: {
      id: "abuse-report-messagebar-error-recent-submit2",
      actionIds: {
        retry: "abuse-report-messagebar-action-retry",
        cancel: "abuse-report-messagebar-action-cancel",
      },
    },
  },
  ERROR_SERVER: {
    actions: ["retry", "cancel"],
    type: "error",
    l10n: {
      id: "abuse-report-messagebar-error2",
      actionIds: {
        retry: "abuse-report-messagebar-action-retry",
        cancel: "abuse-report-messagebar-action-cancel",
      },
    },
  },
  ERROR_UNKNOWN: {
    actions: ["retry", "cancel"],
    type: "error",
    l10n: {
      id: "abuse-report-messagebar-error2",
      actionIds: {
        retry: "abuse-report-messagebar-action-retry",
        cancel: "abuse-report-messagebar-action-cancel",
      },
    },
  },
};

async function openAbuseReport({ addonId, reportEntryPoint }) {
  try {
    const reportDialog = await AbuseReporter.openDialog(
      addonId,
      reportEntryPoint,
      window.docShell.chromeEventHandler
    );

    // Warn the user before the about:addons tab while an
    // abuse report dialog is still open, and close the
    // report dialog if the user choose to close the related
    // about:addons tab.
    const beforeunloadListener = evt => evt.preventDefault();
    const unloadListener = () => reportDialog.close();
    const clearUnloadListeners = () => {
      window.removeEventListener("beforeunload", beforeunloadListener);
      window.removeEventListener("unload", unloadListener);
    };
    window.addEventListener("beforeunload", beforeunloadListener);
    window.addEventListener("unload", unloadListener);

    reportDialog.promiseReport
      .then(
        report => {
          if (report) {
            submitReport({ report });
          }
        },
        err => {
          Cu.reportError(
            `Unexpected abuse report panel error: ${err} :: ${err.stack}`
          );
          reportDialog.close();
        }
      )
      .then(clearUnloadListeners);
  } catch (err) {
    // Log the detailed error to the browser console.
    Cu.reportError(err);
    document.dispatchEvent(
      new CustomEvent("abuse-report:create-error", {
        detail: {
          addonId,
          addon: err.addon,
          errorType: err.errorType,
        },
      })
    );
  }
}

// Unlike the openAbuseReport function, technically this method wouldn't need
// to be async, but it is so that both the implementations will be providing
// the same type signatures (returning a promise) to the callers, independently
// from which abuse reporting feature is enabled.
async function openAbuseReportAMOForm({ addonId, reportEntryPoint }) {
  const amoUrl = AbuseReporter.getAMOFormURL({ addonId });
  windowRoot.ownerGlobal.openTrustedLinkIn(amoUrl, "tab", {
    // Make sure the newly open tab is going to be focused, independently
    // from general user prefs.
    forceForeground: true,
  });
}

window.openAbuseReport = AbuseReporter.amoFormEnabled
  ? openAbuseReportAMOForm
  : openAbuseReport;

// Helper function used to create abuse report message bars in the
// HTML about:addons page.
function createReportMessageBar(
  definitionId,
  { addonId, addonName, addonType },
  { onclose, onaction } = {}
) {
  const barInfo = ABUSE_REPORT_MESSAGE_BARS[definitionId];
  if (!barInfo) {
    throw new Error(`message-bar definition not found: ${definitionId}`);
  }
  const { dismissable, actions, type, l10n } = barInfo;

  // TODO(Bug 1789718): Remove after the deprecated XPIProvider-based
  // implementation is also removed.
  const mappingAddonType =
    addonType === "sitepermission-deprecated" ? "sitepermission" : addonType;

  const getMessageL10n = () => {
    return l10n.idsPerAddonType
      ? l10n.idsPerAddonType[mappingAddonType]
      : l10n.id;
  };
  const getActionL10n = action => {
    return l10n.actionIdsPerAddonType
      ? l10n.actionIdsPerAddonType[mappingAddonType][action]
      : l10n.actionIds[action];
  };

  const messagebar = document.createElement("moz-message-bar");

  document.l10n.setAttributes(messagebar, getMessageL10n(), {
    "addon-name": addonName || addonId,
  });
  messagebar.setAttribute("data-l10n-attrs", "message");

  actions?.forEach(action => {
    const buttonEl = document.createElement("button");
    buttonEl.addEventListener("click", () => onaction && onaction(action));
    document.l10n.setAttributes(buttonEl, getActionL10n(action));
    buttonEl.setAttribute("slot", "actions");
    messagebar.appendChild(buttonEl);
  });

  messagebar.setAttribute("type", type || "info");
  messagebar.dismissable = dismissable;
  messagebar.addEventListener("message-bar:close", onclose, { once: true });

  document.getElementById("abuse-reports-messages").append(messagebar);

  document.dispatchEvent(
    new CustomEvent("abuse-report:new-message-bar", {
      detail: { definitionId, messagebar },
    })
  );
  return messagebar;
}

async function submitReport({ report }) {
  const { addon } = report;
  const addonId = addon.id;
  const addonName = addon.name;
  const addonType = addon.type;

  // Ensure that the tab that originated the report dialog is selected
  // when the user is submitting the report.
  const { gBrowser } = window.windowRoot.ownerGlobal;
  if (gBrowser && gBrowser.getTabForBrowser) {
    let tab = gBrowser.getTabForBrowser(window.docShell.chromeEventHandler);
    gBrowser.selectedTab = tab;
  }

  // Create a message bar while we are still submitting the report.
  const mbSubmitting = createReportMessageBar(
    "submitting",
    { addonId, addonName, addonType },
    {
      onaction: action => {
        if (action === "cancel") {
          report.abort();
          mbSubmitting.remove();
        }
      },
    }
  );

  try {
    await report.submit();
    mbSubmitting.remove();

    // Create a submitted message bar when the submission has been
    // successful.
    let barId;
    if (
      !(addon.permissions & AddonManager.PERM_CAN_UNINSTALL) &&
      !isPending(addon, "uninstall")
    ) {
      // Do not offer remove action if the addon can't be uninstalled.
      barId = "submitted-no-remove-action";
    } else if (report.reportEntryPoint === "uninstall") {
      // With reportEntryPoint "uninstall" a specific message bar
      // is going to be used.
      barId = "submitted-and-removed";
    } else {
      // All the other reportEntryPoint ("menu" and "toolbar_context_menu")
      // use the same kind of message bar.
      barId = "submitted";
    }

    const mbInfo = createReportMessageBar(
      barId,
      {
        addonId,
        addonName,
        addonType,
      },
      {
        onaction: action => {
          mbInfo.remove();
          // action "keep" doesn't require any further action,
          // just handle "remove".
          if (action === "remove") {
            report.addon.uninstall(true);
          }
        },
      }
    );
  } catch (err) {
    // Log the complete error in the console.
    console.error("Error submitting abuse report for", addonId, err);
    mbSubmitting.remove();
    // The report has a submission error, create a error message bar which
    // may optionally allow the user to retry to submit the same report.
    const barId =
      err.errorType in ABUSE_REPORT_MESSAGE_BARS
        ? err.errorType
        : "ERROR_UNKNOWN";

    const mbError = createReportMessageBar(
      barId,
      {
        addonId,
        addonName,
        addonType,
      },
      {
        onaction: action => {
          mbError.remove();
          switch (action) {
            case "retry":
              submitReport({ report });
              break;
            case "cancel":
              report.abort();
              break;
          }
        },
      }
    );
  }
}

document.addEventListener("abuse-report:submit", ({ detail }) => {
  submitReport(detail);
});

document.addEventListener("abuse-report:create-error", ({ detail }) => {
  const { addonId, addon, errorType } = detail;
  const barId =
    errorType in ABUSE_REPORT_MESSAGE_BARS ? errorType : "ERROR_UNKNOWN";
  createReportMessageBar(barId, {
    addonId,
    addonName: addon && addon.name,
    addonType: addon && addon.type,
  });
});
