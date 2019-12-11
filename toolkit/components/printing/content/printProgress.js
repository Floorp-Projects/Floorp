// -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// dialog is just an array we'll use to store various properties from the dialog document...
var dialog;

// the printProgress is a nsIPrintProgress object
var printProgress = null;

// random global variables...
var targetFile;

var docTitle = "";
var docURL = "";
var progressParams = null;
var switchUI = true;

function ellipseString(aStr, doFront) {
  if (!aStr) {
    return "";
  }

  if (
    aStr.length > 3 &&
    (aStr.substr(0, 3) == "..." || aStr.substr(aStr.length - 4, 3) == "...")
  ) {
    return aStr;
  }

  var fixedLen = 64;
  if (aStr.length > fixedLen) {
    if (doFront) {
      var endStr = aStr.substr(aStr.length - fixedLen, fixedLen);
      return "..." + endStr;
    }
    var frontStr = aStr.substr(0, fixedLen);
    return frontStr + "...";
  }
  return aStr;
}

// all progress notifications are done through the nsIWebProgressListener implementation...
var progressListener = {
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      // Put progress meter in undetermined mode.
      dialog.progress.removeAttribute("value");
    }

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      // we are done printing
      // Indicate completion in title area.
      document.l10n.setAttributes(dialog.title, "print-complete");

      // Put progress meter at 100%.
      dialog.progress.setAttribute("value", 100);
      document.l10n.setAttributes(dialog.progressText, "print-percent", {
        percent: 100,
      });

      if (Services.focus.activeWindow == window) {
        // This progress dialog is the currently active window. In
        // this case we need to make sure that some other window
        // gets focus before we close this dialog to work around the
        // buggy Windows XP Fax dialog, which ends up parenting
        // itself to the currently focused window and is unable to
        // survive that window going away. What happens without this
        // opener.focus() call on Windows XP is that the fax dialog
        // is opened only to go away when this dialog actually
        // closes (which can happen asynchronously, so the fax
        // dialog just flashes once and then goes away), so w/o this
        // fix, it's impossible to fax on Windows XP w/o manually
        // switching focus to another window (or holding on to the
        // progress dialog with the mouse long enough).
        opener.focus();
      }

      window.close();
    }
  },

  onProgressChange(
    aWebProgress,
    aRequest,
    aCurSelfProgress,
    aMaxSelfProgress,
    aCurTotalProgress,
    aMaxTotalProgress
  ) {
    if (switchUI) {
      dialog.tempLabel.setAttribute("hidden", "true");
      dialog.progressBox.removeAttribute("hidden");

      switchUI = false;
    }

    if (progressParams) {
      var docTitleStr = ellipseString(progressParams.docTitle, false);
      if (docTitleStr != docTitle) {
        docTitle = docTitleStr;
        dialog.title.value = docTitle;
      }
      var docURLStr = progressParams.docURL;
      if (docURLStr != docURL && dialog.title != null) {
        docURL = docURLStr;
        if (docTitle == "") {
          dialog.title.value = ellipseString(docURLStr, true);
        }
      }
    }

    // Calculate percentage.
    var percent;
    if (aMaxTotalProgress > 0) {
      percent = Math.round((aCurTotalProgress * 100) / aMaxTotalProgress);
      if (percent > 100) {
        percent = 100;
      }

      // Advance progress meter.
      dialog.progress.setAttribute("value", percent);

      // Update percentage label on progress meter.
      document.l10n.setAttributes(dialog.progressText, "print-percent", {
        percent,
      });
    } else {
      // Progress meter should be barber-pole in this case.
      dialog.progress.removeAttribute("value");
      // Update percentage label on progress meter.
      dialog.progressText.setAttribute("value", "");
    }
  },

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    // we can ignore this notification
  },

  onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    if (aMessage != "") {
      dialog.title.setAttribute("value", aMessage);
    }
  },

  onSecurityChange(aWebProgress, aRequest, state) {
    // we can ignore this notification
  },

  onContentBlockingEvent(aWebProgress, aRequest, event) {
    // we can ignore this notification
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),
};

function loadDialog() {}

function onLoad() {
  // Set global variables.
  printProgress = window.arguments[0];
  if (window.arguments[1]) {
    progressParams = window.arguments[1].QueryInterface(
      Ci.nsIPrintProgressParams
    );
    if (progressParams) {
      docTitle = ellipseString(progressParams.docTitle, false);
      docURL = ellipseString(progressParams.docURL, true);
    }
  }

  if (!printProgress) {
    dump("Invalid argument to printProgress.xhtml\n");
    window.close();
    return;
  }

  dialog = {};
  dialog.strings = [];
  dialog.title = document.getElementById("dialog.title");
  dialog.titleLabel = document.getElementById("dialog.titleLabel");
  dialog.progress = document.getElementById("dialog.progress");
  dialog.progressBox = document.getElementById("dialog.progressBox");
  dialog.progressText = document.getElementById("dialog.progressText");
  dialog.progressLabel = document.getElementById("dialog.progressLabel");
  dialog.tempLabel = document.getElementById("dialog.tempLabel");

  dialog.progressBox.setAttribute("hidden", "true");

  document.l10n.setAttributes(dialog.tempLabel, "print-preparing");

  dialog.title.value = docTitle;

  // Fill dialog.
  loadDialog();

  document.addEventListener("dialogcancel", onCancel);
  // set our web progress listener on the helper app launcher
  printProgress.registerListener(progressListener);
  // We need to delay the set title else dom will overwrite it
  window.setTimeout(doneIniting, 500);
}

function onUnload() {
  if (printProgress) {
    try {
      printProgress.unregisterListener(progressListener);
      printProgress = null;
    } catch (exception) {}
  }
}

// If the user presses cancel, tell the app launcher and close the dialog.
function onCancel() {
  // Cancel app launcher.
  try {
    printProgress.processCanceledByUser = true;
  } catch (exception) {}
}

function doneIniting() {
  printProgress.doneIniting();
}
