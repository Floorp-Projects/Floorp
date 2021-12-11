/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides functions to handle status and messages in the user interface.
 */

"use strict";

var EXPORTED_SYMBOLS = ["DownloadUIHelper"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

// BrowserWindowTracker and PrivateBrowsingUtils are only used when opening downloaded files into a browser window
XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

const kStringBundleUrl =
  "chrome://mozapps/locale/downloads/downloads.properties";

const kStringsRequiringFormatting = {
  fileExecutableSecurityWarning: true,
  cancelDownloadsOKTextMultiple: true,
  quitCancelDownloadsAlertMsgMultiple: true,
  quitCancelDownloadsAlertMsgMacMultiple: true,
  offlineCancelDownloadsAlertMsgMultiple: true,
  leavePrivateBrowsingWindowsCancelDownloadsAlertMsgMultiple2: true,
};

/**
 * Provides functions to handle status and messages in the user interface.
 */
var DownloadUIHelper = {
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

  /**
   * Open the given file as a file: URI in the active window
   *
   * @param nsIFile file         The downloaded file
   * @param options.chromeWindow Optional chrome window where we could open the file URI
   * @param options.openWhere    String indicating how to open the URI.
   *                             One of "window", "tab", "tabshifted"
   * @param options.isPrivate    Open in private window or not
   * @param options.browsingContextId BrowsingContext ID of the initiating document
   * @param options.userContextId UserContextID of the initiating document
   */
  loadFileIn(
    file,
    {
      chromeWindow: browserWin,
      openWhere = "tab",
      isPrivate,
      userContextId = 0,
      browsingContextId = 0,
    } = {}
  ) {
    let fileURI = Services.io.newFileURI(file);
    let allowPrivate =
      isPrivate || PrivateBrowsingUtils.permanentPrivateBrowsing;

    if (
      !browserWin ||
      browserWin.document.documentElement.getAttribute("windowtype") !==
        "navigator:browser"
    ) {
      // we'll need a private window for a private download, or if we're in private-only mode
      // but otherwise we want to open files in a non-private window
      browserWin = BrowserWindowTracker.getTopWindow({
        private: allowPrivate,
      });
    }
    // if there is no suitable browser window, we'll need to open one and ignore any other `openWhere` value
    // this can happen if the library dialog is the only open window
    if (!browserWin) {
      // There is no browser window open, so open a new one.
      let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
      let strURI = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      strURI.data = fileURI.spec;
      args.appendElement(strURI);
      let features = "chrome,dialog=no,all";
      if (isPrivate) {
        features += ",private";
      }
      browserWin = Services.ww.openWindow(
        null,
        AppConstants.BROWSER_CHROME_URL,
        null,
        features,
        args
      );
      return;
    }

    // a browser window will have the helpers from utilityOverlay.js
    let browsingContext = browserWin?.BrowsingContext.get(browsingContextId);
    browserWin.openTrustedLinkIn(fileURI.spec, openWhere, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      private: isPrivate,
      userContextId,
      openerBrowser: browsingContext?.top?.embedderElement,
    });
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
  for (let string of sb.getSimpleEnumeration()) {
    let stringName = string.key;
    if (stringName in kStringsRequiringFormatting) {
      strings[stringName] = function() {
        // Convert "arguments" to a real array before calling into XPCOM.
        return sb.formatStringFromName(stringName, Array.from(arguments));
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
var DownloadPrompter = function(aParent) {
  this._prompter = Services.ww.getNewPrompter(aParent);
};

DownloadPrompter.prototype = {
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
   * executable, and asks whether the user wants to launch it.
   *
   * @param path
   *        String containing the full path to the file to be opened.
   *
   * @resolves Boolean indicating whether the launch operation can continue.
   */
  async confirmLaunchExecutable(path) {
    const kPrefSkipConfirm = "browser.download.skipConfirmLaunchExecutable";

    // Always launch in case we have no prompter implementation.
    if (!this._prompter) {
      return true;
    }

    try {
      if (Services.prefs.getBoolPref(kPrefSkipConfirm)) {
        return true;
      }
    } catch (ex) {
      // If the preference does not exist, continue with the prompt.
    }

    let leafName = OS.Path.basename(path);

    let s = DownloadUIHelper.strings;
    return this._prompter.confirm(
      s.fileExecutableSecurityWarningTitle,
      s.fileExecutableSecurityWarning(leafName, leafName)
    );
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
  confirmCancelDownloads: function DP_confirmCancelDownload(
    aDownloadsCount,
    aPromptType
  ) {
    // Always continue in case we have no prompter implementation, or if there
    // are no active downloads.
    if (!this._prompter || aDownloadsCount <= 0) {
      return false;
    }

    let s = DownloadUIHelper.strings;
    let buttonFlags =
      Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0 +
      Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1;
    let okButton =
      aDownloadsCount > 1
        ? s.cancelDownloadsOKTextMultiple(aDownloadsCount)
        : s.cancelDownloadsOKText;
    let title, message, cancelButton;

    switch (aPromptType) {
      case this.ON_QUIT:
        title = s.quitCancelDownloadsAlertTitle;
        if (AppConstants.platform != "macosx") {
          message =
            aDownloadsCount > 1
              ? s.quitCancelDownloadsAlertMsgMultiple(aDownloadsCount)
              : s.quitCancelDownloadsAlertMsg;
          cancelButton = s.dontQuitButtonWin;
        } else {
          message =
            aDownloadsCount > 1
              ? s.quitCancelDownloadsAlertMsgMacMultiple(aDownloadsCount)
              : s.quitCancelDownloadsAlertMsgMac;
          cancelButton = s.dontQuitButtonMac;
        }
        break;
      case this.ON_OFFLINE:
        title = s.offlineCancelDownloadsAlertTitle;
        message =
          aDownloadsCount > 1
            ? s.offlineCancelDownloadsAlertMsgMultiple(aDownloadsCount)
            : s.offlineCancelDownloadsAlertMsg;
        cancelButton = s.dontGoOfflineButton;
        break;
      case this.ON_LEAVE_PRIVATE_BROWSING:
        title = s.leavePrivateBrowsingCancelDownloadsAlertTitle;
        message =
          aDownloadsCount > 1
            ? s.leavePrivateBrowsingWindowsCancelDownloadsAlertMsgMultiple2(
                aDownloadsCount
              )
            : s.leavePrivateBrowsingWindowsCancelDownloadsAlertMsg2;
        cancelButton = s.dontLeavePrivateBrowsingButton2;
        break;
    }

    let rv = this._prompter.confirmEx(
      title,
      message,
      buttonFlags,
      okButton,
      cancelButton,
      null,
      null,
      {}
    );
    return rv == 1;
  },
};
