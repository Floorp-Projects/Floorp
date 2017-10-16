/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides functions to handle status and messages in the user interface.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadUIHelper",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

const kStringBundleUrl =
  "chrome://mozapps/locale/downloads/downloads.properties";

const kStringsRequiringFormatting = {
  fileExecutableSecurityWarning: true,
  cancelDownloadsOKTextMultiple: true,
  quitCancelDownloadsAlertMsgMultiple: true,
  quitCancelDownloadsAlertMsgMacMultiple: true,
  offlineCancelDownloadsAlertMsgMultiple: true,
  leavePrivateBrowsingWindowsCancelDownloadsAlertMsgMultiple2: true
};

/**
 * Provides functions to handle status and messages in the user interface.
 */
this.DownloadUIHelper = {
  /**
   * Returns an object that can be used to display prompts related to downloads.
   *
   * The prompts may be either anchored to a specified window, or anchored to
   * the most recently active window, for example if the prompt is displayed in
   * response to global notifications that are not associated with any window.
   *
   * @param aParent
   *        If specified, should reference the nsIDOMWindow to which the prompts
   *        should be attached.  If omitted, the prompts will be attached to the
   *        most recently active window.
   *
   * @return A DownloadPrompter object.
   */
  getPrompter(aParent) {
    return new DownloadPrompter(aParent || null);
  },
};

/**
 * Returns an object whose keys are the string names from the downloads string
 * bundle, and whose values are either the translated strings or functions
 * returning formatted strings.
 */
XPCOMUtils.defineLazyGetter(DownloadUIHelper, "strings", function() {
  let strings = {};
  let sb = Services.strings.createBundle(kStringBundleUrl);
  let enumerator = sb.getSimpleEnumeration();
  while (enumerator.hasMoreElements()) {
    let string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
    let stringName = string.key;
    if (stringName in kStringsRequiringFormatting) {
      strings[stringName] = function() {
        // Convert "arguments" to a real array before calling into XPCOM.
        return sb.formatStringFromName(stringName,
                                       Array.slice(arguments, 0),
                                       arguments.length);
      };
    } else {
      strings[stringName] = string.value;
    }
  }
  return strings;
});

/**
 * Allows displaying prompts related to downloads.
 *
 * @param aParent
 *        The nsIDOMWindow to which prompts should be attached, or null to
 *        attach prompts to the most recently active window.
 */
this.DownloadPrompter = function(aParent) {
  this._prompter = Services.ww.getNewPrompter(aParent);
};

this.DownloadPrompter.prototype = {
  /**
   * Constants with the different type of prompts.
   */
  ON_QUIT: "prompt-on-quit",
  ON_OFFLINE: "prompt-on-offline",
  ON_LEAVE_PRIVATE_BROWSING: "prompt-on-leave-private-browsing",

  /**
   * nsIPrompt instance for displaying messages.
   */
  _prompter: null,

  /**
   * Displays a warning message box that informs that the specified file is
   * executable, and asks whether the user wants to launch it.  The user is
   * given the option of disabling future instances of this warning.
   *
   * @param aPath
   *        String containing the full path to the file to be opened.
   *
   * @return {Promise}
   * @resolves Boolean indicating whether the launch operation can continue.
   * @rejects JavaScript exception.
   */
  confirmLaunchExecutable(aPath) {
    const kPrefAlertOnEXEOpen = "browser.download.manager.alertOnEXEOpen";

    try {
      // Always launch in case we have no prompter implementation.
      if (!this._prompter) {
        return Promise.resolve(true);
      }

      try {
        if (!Services.prefs.getBoolPref(kPrefAlertOnEXEOpen)) {
          return Promise.resolve(true);
        }
      } catch (ex) {
        // If the preference does not exist, continue with the prompt.
      }

      let leafName = OS.Path.basename(aPath);

      let s = DownloadUIHelper.strings;
      let checkState = { value: false };
      let shouldLaunch = this._prompter.confirmCheck(
                           s.fileExecutableSecurityWarningTitle,
                           s.fileExecutableSecurityWarning(leafName, leafName),
                           s.fileExecutableSecurityWarningDontAsk,
                           checkState);

      if (shouldLaunch) {
        Services.prefs.setBoolPref(kPrefAlertOnEXEOpen, !checkState.value);
      }

      return Promise.resolve(shouldLaunch);
    } catch (ex) {
      return Promise.reject(ex);
    }
  },

  /**
   * Displays a warning message box that informs that there are active
   * downloads, and asks whether the user wants to cancel them or not.
   *
   * @param aDownloadsCount
   *        The current downloads count.
   * @param aPromptType
   *        The type of prompt notification depending on the observer.
   *
   * @return False to cancel the downloads and continue, true to abort the
   *         operation.
   */
  confirmCancelDownloads: function DP_confirmCancelDownload(aDownloadsCount,
                                                            aPromptType) {
    // Always continue in case we have no prompter implementation, or if there
    // are no active downloads.
    if (!this._prompter || aDownloadsCount <= 0) {
      return false;
    }

    let s = DownloadUIHelper.strings;
    let buttonFlags = (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
                      (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1);
    let okButton = aDownloadsCount > 1 ? s.cancelDownloadsOKTextMultiple(aDownloadsCount)
                                       : s.cancelDownloadsOKText;
    let title, message, cancelButton;

    switch (aPromptType) {
      case this.ON_QUIT:
        title = s.quitCancelDownloadsAlertTitle;
        if (AppConstants.platform != "macosx") {
          message = aDownloadsCount > 1
                    ? s.quitCancelDownloadsAlertMsgMultiple(aDownloadsCount)
                    : s.quitCancelDownloadsAlertMsg;
          cancelButton = s.dontQuitButtonWin;
        } else {
          message = aDownloadsCount > 1
                    ? s.quitCancelDownloadsAlertMsgMacMultiple(aDownloadsCount)
                    : s.quitCancelDownloadsAlertMsgMac;
          cancelButton = s.dontQuitButtonMac;
        }
        break;
      case this.ON_OFFLINE:
        title = s.offlineCancelDownloadsAlertTitle;
        message = aDownloadsCount > 1
                  ? s.offlineCancelDownloadsAlertMsgMultiple(aDownloadsCount)
                  : s.offlineCancelDownloadsAlertMsg;
        cancelButton = s.dontGoOfflineButton;
        break;
      case this.ON_LEAVE_PRIVATE_BROWSING:
        title = s.leavePrivateBrowsingCancelDownloadsAlertTitle;
        message = aDownloadsCount > 1
                  ? s.leavePrivateBrowsingWindowsCancelDownloadsAlertMsgMultiple2(aDownloadsCount)
                  : s.leavePrivateBrowsingWindowsCancelDownloadsAlertMsg2;
        cancelButton = s.dontLeavePrivateBrowsingButton2;
        break;
    }

    let rv = this._prompter.confirmEx(title, message, buttonFlags, okButton,
                                      cancelButton, null, null, {});
    return (rv == 1);
  }
};
