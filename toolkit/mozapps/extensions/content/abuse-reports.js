/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint max-len: ["error", 80] */

/* exported openAbuseReport */

/**
 * This script is part of the HTML about:addons page and it provides some
 * helpers used for the Abuse Reporting submission (and related message bars).
 */

// Message Bars definitions.
const ABUSE_REPORT_MESSAGE_BARS = {
  // Idle message-bar (used while the submission is still ongoing).
  "submitting": {id: "submitting", actions: ["cancel"]},
  // Submitted report message-bar.
  "submitted": {
    id: "submitted", actionAddonTypeSuffix: true,
    actions: ["remove", "keep"], dismissable: true,
  },
  // Submitted report message-bar (with no remove actions).
  "submitted-no-remove-action": {
    id: "submitted-noremove", dismissable: true,
  },
  // Submitted report and remove addon message-bar.
  "submitted-and-removed": {
    id: "removed", addonTypeSuffix: true, dismissable: true,
  },
  // The "aborted report" message bar is rendered as a generic informative one,
  // because aborting a report is triggered by a user choice.
  "ERROR_ABORTED_SUBMIT": {
    id: "aborted", type: "generic", dismissable: true,
  },
  // Errors message bars.
  "ERROR_ADDON_NOTFOUND": {
    id: "error", type: "error", dismissable: true,
  },
  "ERROR_CLIENT": {
    id: "error", type: "error", dismissable: true,
  },
  "ERROR_NETWORK": {
    id: "error", actions: ["retry", "cancel"], type: "error",
  },
  "ERROR_RECENT_SUBMIT": {
    id: "error-recent-submit", actions: ["retry", "cancel"], type: "error",
  },
  "ERROR_SERVER": {
    id: "error", actions: ["retry", "cancel"], type: "error",
  },
  "ERROR_UNKNOWN": {
    id: "error", actions: ["retry", "cancel"], type: "error",
  },
};

function openAbuseReport({addonId, reportEntryPoint}) {
  document.dispatchEvent(new CustomEvent("abuse-report:new", {
    detail: {addonId, reportEntryPoint},
  }));
}

// Helper function used to create abuse report message bars in the
// HTML about:addons page.
function createReportMessageBar(
  definitionId, {addonId, addonName, addonType},
  {onclose, onaction} = {}
) {
  const getMessageL10n = (id) => `abuse-report-messagebar-${id}`;
  const getActionL10n = (action) => getMessageL10n(`action-${action}`);

  const barInfo = ABUSE_REPORT_MESSAGE_BARS[definitionId];
  if (!barInfo) {
    throw new Error(`message-bar definition not found: ${definitionId}`);
  }
  const {id, dismissable, actions, type} = barInfo;
  const messageEl = document.createElement("span");

  // The message element includes an addon-name span (also filled by
  // Fluent), which can be used to apply custom styles to the addon name
  // included in the message bar (if needed).
  const addonNameEl = document.createElement("span");
  addonNameEl.setAttribute("data-l10n-name", "addon-name");
  messageEl.append(addonNameEl);

  document.l10n.setAttributes(
    messageEl,
    getMessageL10n(barInfo.addonTypeSuffix ? `${id}-${addonType}` : id),
    {"addon-name": addonName || addonId});

  const barActions = actions ? actions.map(action => {
    // Some of the message bars require a different per addonType
    // Fluent id for their actions.
    const actionId = barInfo.actionAddonTypeSuffix ?
      `${action}-${addonType}` : action;
    const buttonEl = document.createElement("button");
    buttonEl.addEventListener("click", () => onaction && onaction(action));
    document.l10n.setAttributes(buttonEl, getActionL10n(actionId));
    return buttonEl;
  }) : [];

  const messagebar = document.createElement("message-bar");
  messagebar.setAttribute("type", type || "generic");
  if (dismissable) {
    messagebar.setAttribute("dismissable", "");
  }
  messagebar.append(messageEl, ...barActions);
  messagebar.addEventListener("message-bar:close", onclose, {once: true});

  document.getElementById("abuse-reports-messages").append(messagebar);

  document.dispatchEvent(new CustomEvent("abuse-report:new-message-bar", {
    detail: {definitionId, messagebar},
  }));
  return messagebar;
}

async function submitReport({report, reason, message}) {
  const {addon} = report;
  const addonId = addon.id;
  const addonName = addon.name;
  const addonType = addon.type;

  // Create a message bar while we are still submitting the report.
  const mbSubmitting = createReportMessageBar(
    "submitting", {addonId, addonName, addonType}, {
      onaction: (action) => {
        if (action === "cancel") {
          report.abort();
          mbSubmitting.remove();
        }
      },
    });

  try {
    await report.submit({reason, message});
    mbSubmitting.remove();

    // Create a submitted message bar when the submission has been
    // successful.
    let barId;
    if (!(addon.permissions & AddonManager.PERM_CAN_UNINSTALL)) {
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

    const mbInfo = createReportMessageBar(barId, {
      addonId, addonName, addonType,
    }, {
      onaction: (action) => {
        mbInfo.remove();
        // action "keep" doesn't require any further action,
        // just handle "remove".
        if (action === "remove") {
          report.addon.uninstall(true);
        }
      },
    });
  } catch (err) {
    // Log the complete error in the console.
    console.error("Error submitting abuse report for", addonId, err);
    mbSubmitting.remove();
    // The report has a submission error, create a error message bar which
    // may optionally allow the user to retry to submit the same report.
    const barId = err.errorType in ABUSE_REPORT_MESSAGE_BARS ?
      err.errorType : "ERROR_UNKNOWN";

    const mbError = createReportMessageBar(barId, {
      addonId, addonName, addonType,
    }, {
      onaction: (action) => {
        mbError.remove();
        switch (action) {
          case "retry":
            submitReport({report, reason, message});
            break;
          case "cancel":
            report.abort();
            break;
        }
      },
    });
  }
}

document.addEventListener("abuse-report:submit", ({detail}) => {
  submitReport(detail);
});

document.addEventListener("abuse-report:create-error", ({detail}) => {
  const {addonId, addon, errorType} = detail;
  const barId = errorType in ABUSE_REPORT_MESSAGE_BARS ?
    errorType : "ERROR_UNKNOWN";
  createReportMessageBar(barId, {
    addonId,
    addonName: addon && addon.name,
    addonType: addon && addon.type,
  });
});
