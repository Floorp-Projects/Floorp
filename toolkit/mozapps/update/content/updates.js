/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Update Service.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@mozilla.org> (Original Author)
 *  Asaf Romano <mozilla.mano@sent.com>
 *  Jeff Walden <jwalden+code@mit.edu>
 *  Robert Strong <robert.bugzilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/DownloadUtils.jsm");

// Firefox's macBrowserOverlay.xul includes scripts that define Cc, Ci, and Cr
// so we have to use different names.
const CoC = Components.classes;
const CoI = Components.interfaces;
const CoR = Components.results;

const XMLNS_XUL               = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const PREF_UPDATE_MANUAL_URL        = "app.update.url.manual";
const PREF_APP_UPDATE_LOG_BRANCH    = "app.update.log.";
const PREF_UPDATE_TEST_LOOP         = "app.update.test.loop";
const PREF_UPDATE_NEVER_BRANCH      = "app.update.never.";
const PREF_AUTO_UPDATE_ENABLED      = "app.update.enabled";

const UPDATE_TEST_LOOP_INTERVAL     = 2000;

const URI_UPDATES_PROPERTIES  = "chrome://mozapps/locale/update/updates.properties";

const STATE_DOWNLOADING       = "downloading";
const STATE_PENDING           = "pending";
const STATE_APPLYING          = "applying";
const STATE_SUCCEEDED         = "succeeded";
const STATE_DOWNLOAD_FAILED   = "download-failed";
const STATE_FAILED            = "failed";

const SRCEVT_FOREGROUND       = 1;
const SRCEVT_BACKGROUND       = 2;

var gConsole    = null;
var gPref       = null;
var gLogEnabled = { };

/**
 * Logs a string to the error console.
 * @param   string
 *          The string to write to the error console..
 */
function LOG(module, string) {
  if (module in gLogEnabled || "all" in gLogEnabled) {
    dump("*** AUS:UI " + module + ":" + string + "\n");
    gConsole.logStringMessage("AUS:UI " + module + ":" + string);
  }
}

/**
 * Opens a URL using the event target's url attribute for the URL. This is a
 * workaround for Bug 263433 which prevents respecting tab browser preferences
 * for where to open a URL.
 */
function openUpdateURL(event) {
  if (event.button == 0)
    openURL(event.target.getAttribute("url"));
}

/**
 * Gets a preference value, handling the case where there is no default.
 * @param   func
 *          The name of the preference function to call, on nsIPrefBranch
 * @param   preference
 *          The name of the preference
 * @param   defaultValue
 *          The default value to return in the event the preference has
 *          no setting
 * @returns The value of the preference, or undefined if there was no
 *          user or default value.
 */
function getPref(func, preference, defaultValue) {
  try {
    return gPref[func](preference);
  }
  catch (e) {
    LOG("General", "getPref - failed to get preference: " + preference);
  }
  return defaultValue;
}

/**
 * A set of shared data and control functions for the wizard as a whole.
 */
var gUpdates = {
  /**
   * The nsIUpdate object being used by this window (either for downloading,
   * notification or both).
   */
  update: null,

  /**
   * The updates.properties <stringbundle> element.
   */
  strings: null,

  /**
   * The Application brandShortName (e.g. "Firefox")
   */
  brandName: null,

  /**
   * The <wizard> element
   */
  wiz: null,

  /**
   * Helper function for setButtons
   * Resets button to original label & accesskey if string is null.
   */
  _setButton: function(button, string) {
    if (string) {
      var label = this.getAUSString(string);
      if (label.indexOf("%S") != -1)
        label = label.replace(/%S/, this.brandName);
      button.label = label;
      button.setAttribute("accesskey",
                          this.getAUSString(string + ".accesskey"));
    } else {
      button.label = button.defaultLabel;
      button.setAttribute("accesskey", button.defaultAccesskey);
    }
  },

  /**
   * Sets the attributes needed for this Wizard's control buttons (labels,
   * disabled, hidden, etc.)
   * @param   extra1ButtonString
   *          The property in the stringbundle containing the label to put on
   *          the first extra button, or null to hide the first extra button.
   * @param   extra2ButtonString
   *          The property in the stringbundle containing the label to put on
   *          the second extra button, or null to hide the second extra button.
   * @param   nextFinishButtonString
   *          The property in the stringbundle containing the label to put on
   *          the Next / Finish button, or null to hide the button. The Next and
   *          Finish buttons are never displayed at the same time in a wizard
   *          with the the Finish button only being displayed when there are no
   *          additional pages to display in the wizard.
   * @param   canAdvance
   *          true if the wizard can be advanced (e.g. the next / finish button
   *          should be enabled), false otherwise.
   *
   * Note:
   * Per Bug 324121 the wizard should not look like a wizard and to accomplish
   * this the back button is never displayed and the cancel button is only
   * displayed for the checking and the incompatibleCheck pages. This causes the
   * wizard buttons to be arranged as follows on Windows with the next and
   * finish buttons never being displayed at the same time.
   * +--------------------------------------------------------------+
   * | [ extra1 ] [ extra2 ]                     [ next or finish ] |
   * +--------------------------------------------------------------+
   */
  setButtons: function(extra1ButtonString, extra2ButtonString,
                       nextFinishButtonString, canAdvance) {
    this.wiz.canAdvance = canAdvance;

    var bnf = this.wiz.getButton((this.wiz.onLastPage ? "finish" : "next"));
    var be1 = this.wiz.getButton("extra1");
    var be2 = this.wiz.getButton("extra2");

    // Set the labels for the next / finish, extra1, and extra2 buttons
    this._setButton(bnf, nextFinishButtonString);
    this._setButton(be1, extra1ButtonString);
    this._setButton(be2, extra2ButtonString);

    bnf.hidden = !nextFinishButtonString;
    be1.hidden = !extra1ButtonString;
    be2.hidden = !extra2ButtonString;
    // Hide the back button each time setButtons is called (see bug 464765)
    this.wiz.getButton("back").hidden = true;
  },

  getAUSString: function(key, strings) {
    if (strings)
      return this.strings.getFormattedString(key, strings);
    return this.strings.getString(key);
  },

  never: function () {
    // If the user clicks "Never", we should not prompt them about updating to
    // this major update version again, unless they manually do
    // "Check for Updates..." which will clear the "never" pref for the version
    // presented and remind them later about this available update.
    //
    // Encode version since it could be a non-ascii string (bug 359093)
    var neverPrefName = PREF_UPDATE_NEVER_BRANCH +
                       encodeURIComponent(gUpdates.update.version);
    gPref.setBoolPref(neverPrefName, true);
    this.wiz.cancel();
  },

  later: function () {
    // The user said "Later", so close the wizard
    this.wiz.cancel();
  },

  /**
   * A hash of |pageid| attribute to page object. Can be used to dispatch
   * function calls to the appropriate page.
   */
  _pages: { },

  /**
   * Called when the user presses the "Finish" button on the wizard, dispatches
   * the function call to the selected page.
   */
  onWizardFinish: function() {
    var pageid = document.documentElement.currentPage.pageid;
    if ("onWizardFinish" in this._pages[pageid])
      this._pages[pageid].onWizardFinish();
  },

  /**
   * Called when the user presses the "Cancel" button on the wizard, dispatches
   * the function call to the selected page.
   */
  onWizardCancel: function() {
    var pageid = document.documentElement.currentPage.pageid;
    if ("onWizardCancel" in this._pages[pageid])
      this._pages[pageid].onWizardCancel();
  },

  /**
   * Called when the user presses the "Next" button on the wizard, dispatches
   * the function call to the selected page.
   */
  onWizardNext: function() {
    var cp = document.documentElement.currentPage;
    if (!cp)
      return;
    var pageid = cp.pageid;
    if ("onWizardNext" in this._pages[pageid])
      this._pages[pageid].onWizardNext();
  },

  /**
   * The checking process that spawned this update UI. There are two types:
   * SRCEVT_FOREGROUND:
   *   Some user-generated event caused this UI to appear, e.g. the Help
   *   menu item or the button in preferences. When in this mode, the UI
   *   should remain active for the duration of the download.
   * SRCEVT_BACKGROUND:
   *   A background update check caused this UI to appear, probably because
   *   incompatibilities in Extensions or other addons were discovered and
   *   the user's consent to continue was required. When in this mode, the
   *   UI will disappear after the user's consent is obtained.
   */
  sourceEvent: SRCEVT_FOREGROUND,

  /**
   * The global error message - the reason the update failed. This is human
   * readable text, used to initialize the error page.
   */
  errorMessage: "",

  /**
   * Helper function for onLoad
   * Saves default button label & accesskey for use by _setButton
   */
  _cacheButtonStrings: function (buttonName) {
    var button = this.wiz.getButton(buttonName);
    button.defaultLabel = button.label;
    button.defaultAccesskey = button.getAttribute("accesskey");
  },

  /**
   * Called when the wizard UI is loaded.
   */
  onLoad: function() {
    this.wiz = document.documentElement;

    gPref = CoC["@mozilla.org/preferences-service;1"].
            getService(CoI.nsIPrefBranch2);
    gConsole = CoC["@mozilla.org/consoleservice;1"].
               getService(CoI.nsIConsoleService);
    this._initLoggingPrefs();

    this.strings = document.getElementById("updateStrings");
    var brandStrings = document.getElementById("brandStrings");
    this.brandName = brandStrings.getString("brandShortName");

    var pages = gUpdates.wiz.childNodes;
    for (var i = 0; i < pages.length; ++i) {
      var page = pages[i];
      if (page.localName == "wizardpage")
        this._pages[page.pageid] = eval(page.getAttribute("object"));
    }

    // Cache the standard button labels in case we need to restore them
    this._cacheButtonStrings("next");
    this._cacheButtonStrings("finish");
    this._cacheButtonStrings("extra1");
    this._cacheButtonStrings("extra2");

    this.wiz.getButton("cancel").hidden = true;

    // Advance to the Start page.
    var startPage = this.startPage;
    LOG("gUpdates", "onLoad - setting current page to startpage " + startPage.id);
    gUpdates.wiz.currentPage = startPage;
  },

  /**
   * Initialize Logging preferences, formatted like so:
   *  app.update.log.<moduleName> = <true|false>
   */
  _initLoggingPrefs: function() {
    try {
      var ps = CoC["@mozilla.org/preferences-service;1"].
               getService(CoI.nsIPrefService);
      var logBranch = ps.getBranch(PREF_APP_UPDATE_LOG_BRANCH);
      var modules = logBranch.getChildList("", { value: 0 });

      for (var i = 0; i < modules.length; ++i) {
        if (logBranch.prefHasUserValue(modules[i]))
          gLogEnabled[modules[i]] = logBranch.getBoolPref(modules[i]);
      }
    }
    catch (e) {
    }
  },

  /**
   * Return the <wizardpage> object that should be displayed first.
   *
   * This is determined by how we were called by the update prompt:
   *
   * Prompt Method:       Arg0:         Update State: Src Event:  Failed:   Result:
   * showUpdateAvailable  nsIUpdate obj --            background  --        incompatibleCheck
   * showUpdateDownloaded nsIUpdate obj pending       background  --        finishedBackground
   * showUpdateInstalled  "installed"   --            --          --        installed
   * showUpdateError      nsIUpdate obj failed        either      partial   errorpatching
   * showUpdateError      nsIUpdate obj failed        either      complete  errors
   * checkForUpdates      null          --            foreground  --        checking
   * checkForUpdates      null          downloading   foreground  --        downloading
   */
  get startPage() {
    if (window.arguments) {
      var arg0 = window.arguments[0];
      if (arg0 instanceof CoI.nsIUpdate) {
        // If the first argument is a nsIUpdate object, we are notifying the
        // user that the background checking found an update that requires
        // their permission to install, and it's ready for download.
        this.setUpdate(arg0);
        var p = this.update.selectedPatch;
        if (p) {
          var state = p.state;
          if (state == STATE_DOWNLOADING) {
            var patchFailed = false;
            try {
              patchFailed = this.update.getProperty("patchingFailed");
            }
            catch (e) {
            }
            if (patchFailed == "partial") {
              // If the system failed to apply the partial patch, show the
              // screen which best describes this condition, which is triggered
              // by the |STATE_FAILED| state.
              state = STATE_FAILED;
            }
            else if (patchFailed == "complete") {
              // Otherwise, if the complete patch failed, which is far less
              // likely, show the error text held by the update object in the
              // generic errors page, triggered by the |STATE_DOWNLOAD_FAILED|
              // state.
              state = STATE_DOWNLOAD_FAILED;
            }
          }

          // Now select the best page to start with, given the current state of
          // the Update.
          switch (state) {
          case STATE_PENDING:
            this.sourceEvent = SRCEVT_BACKGROUND;
            return document.getElementById("finishedBackground");
          case STATE_DOWNLOADING:
            return document.getElementById("downloading");
          case STATE_FAILED:
            window.getAttention();
            return document.getElementById("errorpatching");
          case STATE_DOWNLOAD_FAILED:
          case STATE_APPLYING:
            return document.getElementById("errors");
          }
        }
        return document.getElementById("incompatibleCheck");
      }
      else if (arg0 == "installed") {
        return document.getElementById("installed");
      }
    }
    else {
      var um = CoC["@mozilla.org/updates/update-manager;1"].
               getService(CoI.nsIUpdateManager);
      if (um.activeUpdate) {
        this.setUpdate(um.activeUpdate);
        return document.getElementById("downloading");
      }
    }
    return document.getElementById("checking");
  },

  /**
   * Sets the Update object for this wizard
   * @param   update
   *          The update object
   */
  setUpdate: function(update) {
    this.update = update;
    if (this.update)
      this.update.QueryInterface(CoI.nsIWritablePropertyBag);
  }
}

/**
 * The "Checking for Updates" page. Provides feedback on the update checking
 * process.
 */
var gCheckingPage = {
  /**
   * The nsIUpdateChecker that is currently checking for updates. We hold onto
   * this so we can cancel the update check if the user closes the window.
   */
  _checker: null,

  /**
   * Initialize
   */
  onPageShow: function() {
    gUpdates.wiz.getButton("cancel").hidden = false;
    gUpdates.setButtons(null, null, null, false);
    this._checker = CoC["@mozilla.org/updates/update-checker;1"].
                    createInstance(CoI.nsIUpdateChecker);
    this._checker.checkForUpdates(this.updateListener, true);
  },

  /**
   * The user has closed the window, either by pressing cancel or using a Window
   * Manager control, so stop checking for updates.
   */
  onWizardCancel: function() {
    this._checker.stopChecking(CoI.nsIUpdateChecker.CURRENT_CHECK);
  },

  /**
   * An object implementing nsIUpdateCheckListener that is notified as the
   * update check commences.
   */
  updateListener: {
    /**
     * See nsIUpdateCheckListener
     */
    onProgress: function(request, position, totalSize) {
      var pm = document.getElementById("checkingProgress");
      checkingProgress.setAttribute("mode", "normal");
      checkingProgress.setAttribute("value", Math.floor(100 * (position / totalSize)));
    },

    /**
     * See nsIUpdateCheckListener
     */
    onCheckComplete: function(request, updates, updateCount) {
      gUpdates.wiz.getButton("cancel").hidden = true;
      var aus = CoC["@mozilla.org/updates/update-service;1"].
                getService(CoI.nsIApplicationUpdateService);
      gUpdates.setUpdate(aus.selectUpdate(updates, updates.length));
      if (gUpdates.update) {
        LOG("gCheckingPage", "onCheckComplete - update found");
        gUpdates.wiz.currentPage.setAttribute("next", "incompatibleCheck");
      }
      else
        LOG("gCheckingPage", "onCheckComplete - no update found");

      gUpdates.wiz.canAdvance = true;
      gUpdates.wiz.advance();
    },

    /**
     * See nsIUpdateCheckListener
     */
    onError: function(request, update) {
      LOG("gCheckingPage", "onError - proceeding to error page");
      gUpdates.wiz.getButton("cancel").hidden = true;
      gUpdates.setUpdate(update);
      gUpdates.wiz.currentPage = document.getElementById("errors");
    },

    /**
     * See nsISupports.idl
     */
    QueryInterface: function(aIID) {
      if (!aIID.equals(CoI.nsIUpdateCheckListener) &&
          !aIID.equals(CoI.nsISupports))
        throw CoR.NS_ERROR_NO_INTERFACE;
      return this;
    }
  }
};

/**
 * The "No Updates Are Available" page
 */
var gNoUpdatesPage = {
  /**
   * Initialize
   */
  onPageShow: function() {
    LOG("gNoUpdatesPage", "onPageShow - could not select an appropriate " +
        "update. Either there were no updates or |selectUpdate| failed");

    if (getPref("getBoolPref", PREF_AUTO_UPDATE_ENABLED, true))
      document.getElementById("noUpdatesAutoEnabled").hidden = false;
    else
      document.getElementById("noUpdatesAutoDisabled").hidden = false;

    gUpdates.setButtons(null, null, "okButton", true);
    gUpdates.wiz.getButton("finish").focus();
  }
};


/**
 * The page that checks if there are any incompatible add-ons.
 */
var gIncompatibleCheckPage = {
  /**
   * List of incompatible add-ons
   */
  addons: [],

  /**
   * Count of incompatible add-ons to check for updates
   */
  _totalCount: 0,

  /**
   * Count of incompatible add-ons that have beend checked for updates
   */
  _completedCount: 0,

  /**
   * The progress bar for this page
   */
  _pBar: null,

  /**
   * Initialize
   */
  onPageShow: function() {
    var ai = CoC["@mozilla.org/xre/app-info;1"].getService(CoI.nsIXULAppInfo);
    var vc = CoC["@mozilla.org/xpcom/version-comparator;1"].
             getService(CoI.nsIVersionComparator);
    if (!gUpdates.update.extensionVersion ||
        vc.compare(gUpdates.update.extensionVersion, ai.version) == 0) {
      // Go to the next page
      gUpdates.wiz.advance();
      return;
    }

    var em = CoC["@mozilla.org/extensions/manager;1"].
             getService(CoI.nsIExtensionManager);
    this.addons = em.getIncompatibleItemList("", gUpdates.update.extensionVersion,
                                             gUpdates.update.platformVersion,
                                             CoI.nsIUpdateItem.TYPE_ANY, false,
                                             { });
    if (this.addons.length > 0) {
      // Don't include add-ons that are already incompatible with the current
      // version of the application
      var addons = em.getIncompatibleItemList("", null, null,
                                              CoI.nsIUpdateItem.TYPE_ANY, false,
                                              { });
      for (var i = 0; i < addons.length; ++i) {
        for (var j = 0; j < this.addons.length; ++j) {
          if (addons[i].id == this.addons[j].id) {
            this.addons.splice(j, 1);
            break;
          }
        }
      }
    }

    if (this.addons.length == 0) {
      // Go to the next page
      gUpdates.wiz.advance();
      return;
    }

    LOG("gIncompatibleCheckPage", "onPageShow - checking for updates to " +
        "incompatible add-ons");

    gUpdates.wiz.getButton("cancel").hidden = false;
    gUpdates.setButtons(null, null, null, false);
    gUpdates.wiz.getButton("cancel").focus();
    this._pBar = document.getElementById("incompatibleCheckProgress");
    this._totalCount = this.addons.length;

    var em = CoC["@mozilla.org/extensions/manager;1"].
             getService(CoI.nsIExtensionManager);
    em.update(this.addons, this.addons.length,
              CoI.nsIExtensionManager.UPDATE_NOTIFY_NEWVERSION, this,
              gUpdates.update.extensionVersion, gUpdates.update.platformVersion);
  },

  /**
   * See nsIExtensionManager.idl
   */
  onUpdateStarted: function() {
    this._pBar.mode = "normal";
  },

  /**
   * See nsIExtensionManager.idl
   */
  onUpdateEnded: function() {
    if (this.addons.length == 0) {
      LOG("gIncompatibleCheckPage", "onUpdateEnded - updates were found " +
          "for all incompatible add-ons");
    }
    else {
      LOG("gIncompatibleCheckPage", "onUpdateEnded - there are still " +
          "incompatible add-ons");
    }

    gUpdates.wiz.getButton("cancel").hidden = true;
    gUpdates.wiz.canAdvance = true;
    gUpdates.wiz.advance();
  },

  /**
   * See nsIExtensionManager.idl
   */
  onAddonUpdateStarted: function(addon) {
  },

  /**
   * See nsIExtensionManager.idl
   */
  onAddonUpdateEnded: function(addon, status) {
    ++this._completedCount;
    this._pBar.value = Math.ceil((this._completedCount / this._totalCount) * 100);

    if (status != CoI.nsIAddonUpdateCheckListener.STATUS_UPDATE &&
        status != CoI.nsIAddonUpdateCheckListener.STATUS_VERSIONINFO)
      return;

    for (var i = 0; i < this.addons.length; ++i) {
      if (this.addons[i].id == addon.id) {
        LOG("gIncompatibleCheckPage", "onAddonUpdateEnded - found update " +
            "for add-on ID: " + addon.id);
        this.addons.splice(i, 1);
        break;
      }
    }
  },

  QueryInterface: function(iid) {
    if (!iid.equals(CoI.nsIAddonUpdateCheckListener) &&
        !iid.equals(CoI.nsISupports))
      throw CoR.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/**
 * The "Updates Are Available" page. Provides the user information about the
 * available update.
 */
var gUpdatesAvailablePage = {
  /**
   * If this page has been previously loaded
   */
  _loaded: false,

  /**
   * Initialize
   */
  onPageShow: function() {
    var severity = gUpdates.update.type;
    gUpdates.setButtons("askLaterButton",
                        severity == "major" ? "noThanksButton" : null,
                        "updateButton_" + severity, true);
    var btn = gUpdates.wiz.getButton("next");
    btn.className += " heed";
    btn.focus();

    if (this._loaded)
      return;

    if (!gUpdates.update.licenseURL) {
      if (gIncompatibleCheckPage.addons.length == 0)
        gUpdates.wiz.currentPage.setAttribute("next", "downloading");
      else
        gUpdates.wiz.currentPage.setAttribute("next", "incompatibleList");
    }

    var updateName = gUpdates.getAUSString("updateName", [gUpdates.brandName,
                                                          gUpdates.update.version]);
    if (gUpdates.update.channel == "nightly")
      updateName = updateName + " " + gUpdates.update.buildID + " nightly";
    var updateNameElement = document.getElementById("updateName");
    updateNameElement.value = updateName;
    var updateTypeElement = document.getElementById("updateType");
    updateTypeElement.setAttribute("severity", severity);

    var moreInfoContent = document.getElementById("moreInfoContent");
    var intro;
    if (severity == "major") {
      // for major updates, use the brandName and the version for the intro
      intro = gUpdates.getAUSString("intro_major_app_and_version",
                                    [gUpdates.brandName, gUpdates.update.version]);
      var remoteContent = document.getElementById("updateMoreInfoContent");
      // update_name and update_version need to be set before url
      // so that when attempting to download the url, we can show
      // the formatted "Download..." string
      remoteContent.update_name = gUpdates.brandName;
      remoteContent.update_version = gUpdates.update.version;
      remoteContent.url = gUpdates.update.detailsURL;

      moreInfoContent.hidden = false;
      document.getElementById("moreInfoURL").hidden = true;
      document.getElementById("updateName").hidden = true;
      document.getElementById("updateNameSep").hidden = true;
      document.getElementById("upgradeEvangelism").hidden = true;
      document.getElementById("upgradeEvangelismSep").hidden = true;

      // Clear the "never" pref for this version.  this is to handle the
      // scenario where the user clicked "never" for a major update and then at
      // a later point, did "Check for Updates..." and then canceled.  If we
      // don't clear the "never" pref future notifications for this version
      // will never happen.
      //
      // Encode version since it could be a non-ascii string (bug 359093)
      var neverPrefName = PREF_UPDATE_NEVER_BRANCH +
                          encodeURIComponent(gUpdates.update.version);
      gPref.setBoolPref(neverPrefName, false);
    }
    else {
      // for minor updates, just use the brandName for the intro
      intro = gUpdates.getAUSString("intro_minor_app", [gUpdates.brandName]);
      // This element when hidden still receives focus events which will
      // cause assertions with debug builds so remove it if it isn't used.
      moreInfoContent.parentNode.removeChild(moreInfoContent);
      var updateMoreInfoURL = document.getElementById("updateMoreInfoURL");
      updateMoreInfoURL.setAttribute("url", gUpdates.update.detailsURL);
    }
    updateTypeElement.textContent = intro;

    var updateTitle = gUpdates.getAUSString("updatesfound_" + severity +
                                            ".title");
    gUpdates.wiz.currentPage.setAttribute("label", updateTitle);
    // this is necessary to make this change to the label of the current
    // wizard page show up
    gUpdates.wiz._adjustWizardHeader();

    this._loaded = true;
  },

  onExtra1: function() {
    this.onWizardCancel();
    gUpdates.later();
  },

  onExtra2: function() {
    this.onWizardCancel();
    gUpdates.never();
  },

  onWizardNext: function() {
    var regex = new RegExp('\\s*heed');
    var btn = gUpdates.wiz.getButton("next");
    btn.className = btn.className.replace(regex, "");
  },

  /**
   * When the user cancels the wizard
   */
  onWizardCancel: function() {
    try {
      var remoteContent = document.getElementById("updateMoreInfoContent");
      if (remoteContent)
        remoteContent.stopDownloading();
    }
    catch (e) {
      LOG("gUpdatesAvailablePage", "onWizardCancel - " +
          "moreInfoContent.stopDownloading() failed: " + e);
    }
  }
};

/**
 * The page which shows the user a license associated with an update. The
 * user must agree to the terms of the license before continuing to install
 * the update.
 */
var gLicensePage = {
  /**
   * If the license url has been previously loaded
   */
  _licenseLoaded: false,

  /**
   * Initialize
   */
  onPageShow: function() {
    gUpdates.setButtons("backButton", null, "acceptTermsButton", false);

    var licenseContent = document.getElementById("licenseContent");
    if (this._licenseLoaded || licenseContent.getAttribute("state") == "error") {
      this.onAcceptDeclineRadio();
      var licenseGroup = document.getElementById("acceptDeclineLicense");
      if (licenseGroup.selectedIndex == 0)
        gUpdates.wiz.getButton("next").focus();
      else
        licenseGroup.focus();
      return;
    }

    gUpdates.wiz.getButton("extra1").disabled = true;
    if (gIncompatibleCheckPage.addons.length == 0)
      gUpdates.wiz.currentPage.setAttribute("next", "downloading");

    // Disable the license radiogroup until the EULA has been downloaded
    document.getElementById("acceptDeclineLicense").disabled = true;
    gUpdates.update.setProperty("licenseAccepted", "false");

    licenseContent.addEventListener("load", gLicensePage.onLicenseLoad, false);
    // The update_name and update_version need to be set before url so the ui
    // can display the formatted "Download..." string when attempting to
    // download the url.
    licenseContent.update_name = gUpdates.brandName;
    licenseContent.update_version = gUpdates.update.version;
    licenseContent.url = gUpdates.update.licenseURL;
  },

  /**
   * When the license document has loaded
   */
  onLicenseLoad: function() {
    var licenseContent = document.getElementById("licenseContent");
    // Disable or enable the radiogroup based on the state attribute of
    // licenseContent.
    // Note: may be called multiple times due to multiple onLoad events.
    var state = licenseContent.getAttribute("state");
    if (state == "loading")
      return;

    licenseContent.removeEventListener("load", gLicensePage.onLicenseLoad, false);

    var errorLoading = (state == "error");
    document.getElementById("acceptDeclineLicense").disabled = errorLoading;
    gLicensePage._licenseLoaded = !errorLoading;
    gUpdates.wiz.getButton("extra1").disabled = false;
  },

  /**
   * When the user changes the state of the accept / decline radio group
   */
  onAcceptDeclineRadio: function() {
    // Return early if this page hasn't been loaded (bug 405257). This event is
    // fired during the construction of the wizard before gUpdates has received
    // its onload event (bug 452389).
    if (!this._licenseLoaded)
      return;

    var selectedIndex = document.getElementById("acceptDeclineLicense")
                                .selectedIndex;
    // 0 == Accept, 1 == Decline
    var licenseAccepted = (selectedIndex == 0);
    gUpdates.wiz.canAdvance = licenseAccepted;
  },

  onExtra1: function() {
    gUpdates.wiz.currentPage = document.getElementById("updatesfound");
  },

  /**
   * When the user accepts the license by hitting "Next"
   */
  onWizardNext: function() {
    try {
      gUpdates.update.setProperty("licenseAccepted", "true");
      var um = CoC["@mozilla.org/updates/update-manager;1"].
               getService(CoI.nsIUpdateManager);
      um.saveUpdates();
    }
    catch (e) {
      LOG("gLicensePage", "onWizardNext - nsIUpdateManager:saveUpdates() " +
          "failed: " + e);
    }
  },

  /**
   * When the user cancels the wizard
   */
  onWizardCancel: function() {
    try {
      var licenseContent = document.getElementById("licenseContent");
      // If the license was downloading, stop it.
      if (licenseContent)
        licenseContent.stopDownloading();
    }
    catch (e) {
      LOG("gLicensePage", "onWizardCancel - " +
          "licenseContent.stopDownloading() failed: " + e);
    }
  }
};

/**
 * The page which shows add-ons that are incompatible and do not have updated
 * compatibility information or a version update available to make them
 * compatible.
 */
var gIncompatibleListPage = {
  /**
   * Initialize
   */
  onPageShow: function() {
    gUpdates.setButtons("backButton", null, "okButton", true);
    var listbox = document.getElementById("incompatibleList.listbox");
    if (listbox.children.length > 0)
      return;

    var severity = gUpdates.update.type;
    var intro;
    if (severity == "major")
      intro = gUpdates.getAUSString("incompatibleAddons_" + severity,
                                    [gUpdates.brandName, gUpdates.update.version,
                                     gUpdates.brandName]);
    else
      intro = gUpdates.getAUSString("incompatibleAddons_" + severity,
                                    [gUpdates.brandName]);

    document.getElementById("incompatibleListDesc").textContent = intro;

    var addons = gIncompatibleCheckPage.addons;
    for (var i = 0; i < addons.length; ++i) {
      var listitem = document.createElement("listitem");
      listitem.setAttribute("label", addons[i].name + " " + addons[i].version);
      listbox.appendChild(listitem);
    }
  },

  onExtra1: function() {
    var updatesfoundPage = document.getElementById("updatesfound");
    if (updatesfoundPage.getAttribute("next") == "license")
      gUpdates.wiz.currentPage = document.getElementById("license");
    else
      gUpdates.wiz.currentPage = updatesfoundPage;
  }
};

/**
 * The "Update is Downloading" page - provides feedback for the download
 * process plus a pause/resume UI
 */
var gDownloadingPage = {
  /**
   * DOM Elements
   */
  _downloadName: null,
  _downloadStatus: null,
  _downloadProgress: null,
  _downloadThrobber: null,
  _pauseButton: null,

  /**
   * Whether or not we are currently paused
   */
  _paused: false,

  /**
   * Label cache to hold the 'Connecting' string
   */
  _label_downloadStatus: null,

  /**
   * Member variables for updating download status
   */
  _lastSec: Infinity,
  _startTime: Date.now(),
  _pausedStatus: "",

  /**
   * Initialize
   */
  onPageShow: function() {
    this._downloadName = document.getElementById("downloadName");
    this._downloadStatus = document.getElementById("downloadStatus");
    this._downloadProgress = document.getElementById("downloadProgress");
    this._downloadThrobber = document.getElementById("downloadThrobber");
    this._pauseButton = document.getElementById("pauseButton");
    this._label_downloadStatus = this._downloadStatus.textContent;

    this._pauseButton.setAttribute("tooltiptext",
                                   gUpdates.getAUSString("pauseButtonPause"));

    // move focus to the pause/resume button and then disable it (bug #353177)
    this._pauseButton.focus();
    this._pauseButton.disabled = true;

    var aus = CoC["@mozilla.org/updates/update-service;1"].
              getService(CoI.nsIApplicationUpdateService);

    var um = CoC["@mozilla.org/updates/update-manager;1"].
             getService(CoI.nsIUpdateManager);
    var activeUpdate = um.activeUpdate;
    if (activeUpdate)
      gUpdates.setUpdate(activeUpdate);

    if (!gUpdates.update) {
      LOG("gDownloadingPage", "onPageShow - no valid update to download?!");
      return;
    }

    try {
      // Say that this was a foreground download, not a background download,
      // since the user cared enough to look in on this process.
      gUpdates.update.QueryInterface(CoI.nsIWritablePropertyBag);
      gUpdates.update.setProperty("foregroundDownload", "true");

      // Pause any active background download and restart it as a foreground
      // download.
      aus.pauseDownload();
      var state = aus.downloadUpdate(gUpdates.update, false);
      if (state == "failed") {
        // We've tried as hard as we could to download a valid update -
        // we fell back from a partial patch to a complete patch and even
        // then we couldn't validate. Show a validation error with instructions
        // on how to manually update.
        this.showVerificationError();
      }
      else {
        // Add this UI as a listener for active downloads
        aus.addDownloadListener(this);
      }

      if (activeUpdate)
        this._setUIState(!aus.isDownloading);
    }
    catch(e) {
      LOG("gDownloadingPage", "onPageShow - error: " + e);
    }

    var link = document.getElementById("downloadDetailsLink");
    link.setAttribute("url", gUpdates.update.detailsURL);

    gUpdates.setButtons("hideButton", null, null, false);
    gUpdates.wiz.getButton("extra1").focus();
  },

  /**
   * Updates the text status message
   */
  _setStatus: function(status) {
    // Don't bother setting the same text more than once. This can happen
    // due to the asynchronous behavior of the downloader.
    if (this._downloadStatus.textContent == status)
      return;
    while (this._downloadStatus.hasChildNodes())
      this._downloadStatus.removeChild(this._downloadStatus.firstChild);
    this._downloadStatus.appendChild(document.createTextNode(status));
  },

  /**
   * Update download progress status to show time left, speed, and progress.
   * Also updates the status needed for pausing the download.
   *
   * @param aCurr
   *        Current number of bytes transferred
   * @param aMax
   *        Total file size of the download
   * @return Current active download status
   */
  _updateDownloadStatus: function(aCurr, aMax) {
    let status;

    // Get the download time left and progress
    let rate = aCurr / (Date.now() - this._startTime) * 1000;
    [status, this._lastSec] =
      DownloadUtils.getDownloadStatus(aCurr, aMax, rate, this._lastSec);

    // Get the download progress for pausing
    this._pausedStatus = DownloadUtils.getTransferTotal(aCurr, aMax);

    return status;
  },

  /**
   * Adjust UI to suit a certain state of paused-ness
   * @param   paused
   *          Whether or not the download is paused
   */
  _setUIState: function(paused) {
    var u = gUpdates.update;
    if (paused) {
      if (this._downloadThrobber.hasAttribute("state"))
        this._downloadThrobber.removeAttribute("state");
      if (this._downloadProgress.mode != "normal")
        this._downloadProgress.mode = "normal";
      this._downloadName.value = gUpdates.getAUSString("pausedName", [u.name]);
      this._pauseButton.setAttribute("tooltiptext",
                                     gUpdates.getAUSString("pauseButtonResume"));
      this._pauseButton.setAttribute("paused", "true");
      var p = u.selectedPatch.QueryInterface(CoI.nsIPropertyBag);
      var status = p.getProperty("status");
      if (status) {
        let pausedStatus = gUpdates.getAUSString("pausedStatus", [status]);
        this._setStatus(pausedStatus);
      }
    }
    else {
      if (!(this._downloadThrobber.hasAttribute("state") &&
           (this._downloadThrobber.getAttribute("state") == "loading")))
        this._downloadThrobber.setAttribute("state", "loading");
      if (this._downloadProgress.mode != "undetermined")
        this._downloadProgress.mode = "undetermined";
      this._downloadName.value = gUpdates.getAUSString("downloadingPrefix",
                                                       [u.name]);
      this._pauseButton.setAttribute("paused", "false");
      this._pauseButton.setAttribute("tooltiptext",
                                     gUpdates.getAUSString("pauseButtonPause"));
      this._setStatus(this._label_downloadStatus);
    }
  },

  /**
   * When the user clicks the Pause/Resume button
   */
  onPause: function() {
    var aus = CoC["@mozilla.org/updates/update-service;1"].
              getService(CoI.nsIApplicationUpdateService);
    if (this._paused)
      aus.downloadUpdate(gUpdates.update, false);
    else {
      var patch = gUpdates.update.selectedPatch;
      patch.QueryInterface(CoI.nsIWritablePropertyBag);
      patch.setProperty("status", this._pausedStatus);
      aus.pauseDownload();
    }
    this._paused = !this._paused;

    // Update the UI
    this._setUIState(this._paused);
  },

  /**
   * When the user closes the Wizard UI by clicking the Hide button
   */
  onHide: function() {
    // Remove ourself as a download listener so that we don't continue to be
    // fed progress and state notifications after the UI we're updating has
    // gone away.
    var aus = CoC["@mozilla.org/updates/update-service;1"].
              getService(CoI.nsIApplicationUpdateService);
    aus.removeDownloadListener(this);

    var um = CoC["@mozilla.org/updates/update-manager;1"].
             getService(CoI.nsIUpdateManager);
    um.activeUpdate = gUpdates.update;

    // If the download was paused by the user, ask the user if they want to
    // have the update resume in the background.
    var downloadInBackground = true;
    if (this._paused) {
      var title = gUpdates.getAUSString("resumePausedAfterCloseTitle");
      var message = gUpdates.getAUSString("resumePausedAfterCloseMsg",
                                          [gUpdates.brandName]);
      var ps = CoC["@mozilla.org/embedcomp/prompt-service;1"].
               getService(CoI.nsIPromptService);
      var flags = ps.STD_YES_NO_BUTTONS;
      // Focus the software update wizard before prompting. Tthis will raise
      // the software update wizard if it is minimized making it more obvious
      // what the prompt is for and will solve the problem of windows
      // obscuring the prompt. See bug #350299 for more details.
      window.focus();
      var rv = ps.confirmEx(window, title, message, flags, null, null, null,
                            null, { });
      if (rv == CoI.nsIPromptService.BUTTON_POS_0)
        downloadInBackground = false;
    }
    if (downloadInBackground) {
      // Continue download in the background at full speed.
      LOG("gDownloadingPage", "onHide - continuing download in background " +
          "at full speed");
      aus.downloadUpdate(gUpdates.update, false);
    }
    gUpdates.wiz.cancel();
  },

  /**
   * When the data transfer begins
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   */
  onStartRequest: function(request, context) {
    if (request instanceof CoI.nsIIncrementalDownload)
      LOG("gDownloadingPage", "onStartRequest - spec: " + request.URI.spec);
    // This !paused test is necessary because onStartRequest may fire after
    // the download was paused (for those speedy clickers...)
    if (this._paused)
      return;

    if (!(this._downloadThrobber.hasAttribute("state") &&
          (this._downloadThrobber.getAttribute("state") == "loading")))
      this._downloadThrobber.setAttribute("state", "loading");
    if (this._downloadProgress.mode != "undetermined")
      this._downloadProgress.mode = "undetermined";
    this._setStatus(this._label_downloadStatus);
  },

  /**
   * When new data has been downloaded
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   progress
   *          The current number of bytes transferred
   * @param   maxProgress
   *          The total number of bytes that must be transferred
   */
  onProgress: function(request, context, progress, maxProgress) {
    LOG("gDownloadingPage", "onProgress - progress: " + progress + "/" +
        maxProgress);

    var name = gUpdates.getAUSString("downloadingPrefix", [gUpdates.update.name]);
    let status = this._updateDownloadStatus(progress, maxProgress);
    var currentProgress = Math.round(100 * (progress / maxProgress));

    var p = gUpdates.update.selectedPatch;
    p.QueryInterface(CoI.nsIWritablePropertyBag);
    p.setProperty("progress", currentProgress);
    p.setProperty("status", status);

    // This !paused test is necessary because onProgress may fire after
    // the download was paused (for those speedy clickers...)
    if (this._paused)
      return;

    if (!(this._downloadThrobber.hasAttribute("state") &&
         (this._downloadThrobber.getAttribute("state") == "loading")))
      this._downloadThrobber.setAttribute("state", "loading");
    if (this._downloadProgress.mode != "normal")
      this._downloadProgress.mode = "normal";
    this._downloadProgress.value = currentProgress;
    this._pauseButton.disabled = false;
    this._downloadName.value = name;
    this._setStatus(status);
  },

  /**
   * When we have new status text
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   status
   *          A status code
   * @param   statusText
   *          Human readable version of |status|
   */
  onStatus: function(request, context, status, statusText) {
    LOG("gDownloadingPage", "onStatus - status: " + status + ", text: " +
        statusText);
    this._setStatus(statusText);
  },

  /**
   * When data transfer ceases
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   status
   *          Status code containing the reason for the cessation.
   */
  onStopRequest: function(request, context, status) {
    if (request instanceof CoI.nsIIncrementalDownload)
      LOG("gDownloadingPage", "onStopRequest - spec: " + request.URI.spec +
          ", status: " + status);

    if (this._downloadThrobber.hasAttribute("state"))
      this._downloadThrobber.removeAttribute("state");
    if (this._downloadProgress.mode != "normal")
      this._downloadProgress.mode = "normal";

    var u = gUpdates.update;
    switch (status) {
    case CoR.NS_ERROR_UNEXPECTED:
      if (u.selectedPatch.state == STATE_DOWNLOAD_FAILED &&
          u.isCompleteUpdate) {
        // Verification error of complete patch, informational text is held in
        // the update object.
        gUpdates.wiz.currentPage = document.getElementById("errors");
      }
      else {
        // Verification failed for a partial patch, complete patch is now
        // downloading so return early and do NOT remove the download listener!

        // Reset the progress meter to "undertermined" mode so that we don't
        // show old progress for the new download of the "complete" patch.
        this._downloadProgress.mode = "undetermined";
        this._pauseButton.disabled = true;

        var verificationFailed = document.getElementById("verificationFailed");
        verificationFailed.hidden = false;

        return;
      }
      break;
    case CoR.NS_BINDING_ABORTED:
      LOG("gDownloadingPage", "onStopRequest - pausing download");
      // Return early, do not remove UI listener since the user may resume
      // downloading again.
      return;
    case CoR.NS_OK:
      LOG("gDownloadingPage", "onStopRequest - patch verification succeeded");
      gUpdates.wiz.canAdvance = true;
      gUpdates.wiz.advance();
      break;
    default:
      LOG("gDownloadingPage", "onStopRequest - transfer failed");
      // Some kind of transfer error, die.
      gUpdates.wiz.currentPage = document.getElementById("errors");
      break;
    }

    var aus = CoC["@mozilla.org/updates/update-service;1"].
              getService(CoI.nsIApplicationUpdateService);
    aus.removeDownloadListener(this);
  },

  /**
   * See nsISupports.idl
   */
  QueryInterface: function(iid) {
    if (!iid.equals(CoI.nsIRequestObserver) &&
        !iid.equals(CoI.nsIProgressEventSink) &&
        !iid.equals(CoI.nsISupports))
      throw CoR.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/**
 * The "There was an error during the update" page.
 */
var gErrorsPage = {
  /**
   * Initialize
   */
  onPageShow: function() {
    // If opened after a download completes the cancel button needs to be hidden
    gUpdates.setButtons(null, null, "okButton", true);
    gUpdates.wiz.getButton("finish").focus();

    var statusText = gUpdates.update.statusText;
    LOG("gErrorsPage" , "onPageShow - update.statusText: " + statusText);

    var errorReason = document.getElementById("errorReason");
    errorReason.value = statusText;
    var formatter = CoC["@mozilla.org/toolkit/URLFormatterService;1"].
                    getService(CoI.nsIURLFormatter);
    var manualURL = formatter.formatURLPref(PREF_UPDATE_MANUAL_URL);
    var errorLinkLabel = document.getElementById("errorLinkLabel");
    errorLinkLabel.value = manualURL;
    errorLinkLabel.setAttribute("url", manualURL);
  },

  /**
   * Finish button clicked.
   */
  onWizardFinish: function() {
    // XXXjwalden COMPLETE AND TOTAL HACK!!!
    //
    // The problem the following code is working around is this: the update
    // service's API for responding to updates is poor.  Essentially, all
    // the information we can get is that we've started to request a download or
    // that the download we've started has finished, with minimal details about
    // how it finished which aren't described in the API (and which internally
    // are not entirely useful, either -- mostly unuseful nsresults).  How
    // do you signal the difference between "this download failed" and "all
    // downloads failed", particularly if you aim for API compatibility?  The
    // code in nsApplicationUpdateService only determines the difference *after*
    // the current request is stopped, and since the subsequent second call to
    // downloadUpdate doesn't start/stop a request, the download listener is
    // never notified and whatever code was relying on it just fails without
    // notification.  The consequence of this is that it's impossible to
    // properly remove the download listener.
    //
    // The code before this patch tried to do the exit after all downloads
    // failed but was missing a QueryInterface to work; with it, making sure
    // that the download listener is removed in all cases, including in the case
    // where the last onStopRequest corresponds to *a* failed download instead
    // of to *all* failed downloads, simply means that we have to try to remove
    // that listener in the error page spawned by the update service.  If there
    // were time and API compat weren't a problem, we'd add an onFinish handler
    // or something which could signal exactly what happened and not overload
    // onStopRequest, but there isn't, so we can't.
    //
    var aus = CoC["@mozilla.org/updates/update-service;1"].
              getService(CoI.nsIApplicationUpdateService);
    aus.removeDownloadListener(gDownloadingPage);
  }
};

/**
 * The "Update has been downloaded" page. Shows information about what
 * was downloaded.
 */
var gFinishedPage = {
  /**
   * Initialize
   */
  onPageShow: function() {
    gUpdates.setButtons("restartLaterButton", null, "restartNowButton",
                        true);
    var btn = gUpdates.wiz.getButton("finish");
    btn.className += " heed";
    btn.focus();
  },

  /**
   * Initialize the Wizard Page for a Background Source Event
   */
  onPageShowBackground: function() {
    this.onPageShow();
    var updateFinishedName = document.getElementById("updateFinishedName");
    updateFinishedName.value = gUpdates.update.name;

    var link = document.getElementById("finishedBackgroundLink");
    link.setAttribute("url", gUpdates.update.detailsURL);
    // The details link is stealing focus so it is disabled by default and
    // should only be enabled after onPageShow has been called.
    link.disabled = false;

    if (getPref("getBoolPref", PREF_UPDATE_TEST_LOOP, false)) {
      setTimeout(function () {
                   gUpdates.wiz.getButton("finish").click();
                 }, UPDATE_TEST_LOOP_INTERVAL);
    }
  },

  /**
   * Called when the wizard finishes, i.e. the "Restart Now" button is
   * clicked.
   */
  onWizardFinish: function() {
    // Do the restart
    LOG("gFinishedPage" , "onWizardFinish - restarting the application");

    // disable the "finish" (Restart) and "extra1" (Later) buttons
    // because the Software Update wizard is still up at the point,
    // and will remain up until we return and we close the
    // window with a |window.close()| in wizard.xml
    // (it was the firing the "wizardfinish" event that got us here.)
    // This prevents the user from switching back
    // to the Software Update dialog and clicking "Restart" or "Later"
    // when dealing with the "confirm close" prompts.
    // See bug #350299 for more details.
    gUpdates.wiz.getButton("finish").disabled = true;
    gUpdates.wiz.getButton("extra1").disabled = true;

    // Notify all windows that an application quit has been requested.
    var os = CoC["@mozilla.org/observer-service;1"].
             getService(CoI.nsIObserverService);
    var cancelQuit = CoC["@mozilla.org/supports-PRBool;1"].
                     createInstance(CoI.nsISupportsPRBool);
    os.notifyObservers(cancelQuit, "quit-application-requested", "restart");

    // Something aborted the quit process.
    if (cancelQuit.data)
      return;

    // Restart the application
    CoC["@mozilla.org/toolkit/app-startup;1"].getService(CoI.nsIAppStartup).
    quit(CoI.nsIAppStartup.eAttemptQuit | CoI.nsIAppStartup.eRestart);
  },

  /**
   * When the user clicks the "Restart Later" instead of the Restart Now" button
   * in the wizard after an update has been downloaded.
   */
  onExtra1: function() {
    var updatePrompt = CoC["@mozilla.org/updates/update-prompt;1"].
                       createInstance(CoI.nsIUpdatePrompt);
    // XXXrstrong - showUpdateDownloaded is broken (see bug 462568)
    updatePrompt.showUpdateDownloaded(gUpdates.update, true);
    gUpdates.wiz.cancel();
  }
};

/**
 * The "Update was Installed Successfully" page.
 */
var gInstalledPage = {
  /**
   * Initialize
   */
  onPageShow: function() {
    var ai = CoC["@mozilla.org/xre/app-info;1"].getService(CoI.nsIXULAppInfo);

    var branding = document.getElementById("brandStrings");
    try {
      var url = branding.getFormattedString("whatsNewURL", [ai.version]);
      var whatsnewLink = document.getElementById("whatsnewLink");
      whatsnewLink.setAttribute("url", url);
      whatsnewLink.hidden = false;
    }
    catch (e) {
    }

    gUpdates.setButtons(null, null, "okButton", true);
    gUpdates.wiz.getButton("finish").focus();
  }
};

/**
 * Called as the application shuts down due to being quit from the File->Quit
 * menu item.
 */
function tryToClose() {
  var cp = gUpdates.wiz.currentPage;
  if (cp.pageid != "finished" && cp.pageid != "finishedBackground")
    gUpdates.onWizardCancel();
  return true;
}

/**
 * Callback for the Update Prompt to set the current page if an Update Wizard
 * window is already found to be open.
 * @param   pageid
 *          The ID of the page to switch to
 */
function setCurrentPage(pageid) {
  gUpdates.wiz.currentPage = document.getElementById(pageid);
}
