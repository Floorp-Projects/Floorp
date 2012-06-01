/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

// Firefox's macBrowserOverlay.xul includes scripts that define Cc, Ci, and Cr
// so we have to use different names.
const CoC = Components.classes;
const CoI = Components.interfaces;
const CoR = Components.results;

const XMLNS_XUL               = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const PREF_APP_UPDATE_BACKGROUNDERRORS   = "app.update.backgroundErrors";
const PREF_APP_UPDATE_BILLBOARD_TEST_URL = "app.update.billboard.test_url";
const PREF_APP_UPDATE_CERT_ERRORS        = "app.update.cert.errors";
const PREF_APP_UPDATE_ENABLED            = "app.update.enabled";
const PREF_APP_UPDATE_LOG                = "app.update.log";
const PREF_APP_UPDATE_MANUAL_URL         = "app.update.url.manual";
const PREF_APP_UPDATE_NEVER_BRANCH       = "app.update.never.";
const PREF_APP_UPDATE_TEST_LOOP          = "app.update.test.loop";
const PREF_PLUGINS_UPDATEURL             = "plugins.update.url";

const PREF_EM_HOTFIX_ID                  = "extensions.hotfix.id";

const UPDATE_TEST_LOOP_INTERVAL     = 2000;

const URI_UPDATES_PROPERTIES  = "chrome://mozapps/locale/update/updates.properties";

const STATE_DOWNLOADING       = "downloading";
const STATE_PENDING           = "pending";
const STATE_PENDING_SVC       = "pending-service";
const STATE_APPLYING          = "applying";
const STATE_APPLIED           = "applied";
const STATE_APPLIED_SVC       = "applied-service";
const STATE_SUCCEEDED         = "succeeded";
const STATE_DOWNLOAD_FAILED   = "download-failed";
const STATE_FAILED            = "failed";

const SRCEVT_FOREGROUND       = 1;
const SRCEVT_BACKGROUND       = 2;

const CERT_ATTR_CHECK_FAILED_NO_UPDATE  = 100;
const CERT_ATTR_CHECK_FAILED_HAS_UPDATE = 101;
const BACKGROUNDCHECK_MULTIPLE_FAILURES = 110;

var gLogEnabled = false;
var gUpdatesFoundPageId;

// Notes:
// 1. use the wizard's goTo method whenever possible to change the wizard
//    page since it is simpler than most other methods and behaves nicely with
//    mochitests.
// 2. using a page's onPageShow method to then change to a different page will
//    of course call that page's onPageShow method which can make mochitests
//    overly complicated and fragile so avoid doing this if at all possible.
//    This is why a page's next attribute is set prior to the page being shown
//    whenever possible.

/**
 * Logs a string to the error console.
 * @param   string
 *          The string to write to the error console..
 */
function LOG(module, string) {
  if (gLogEnabled) {
    dump("*** AUS:UI " + module + ":" + string + "\n");
    Services.console.logStringMessage("AUS:UI " + module + ":" + string);
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
    return Services.prefs[func](preference);
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
   * List of incompatible add-ons
   */
  addons: [],

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
   * Whether to run the unload handler. This will be set to false when the user
   * exits the wizard via onWizardCancel or onWizardFinish.
   */
  _runUnload: true,

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
   * @param   showCancel
   *          true if the wizard's cancel button should be shown, false
   *          otherwise. If not specified this will default to false.
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
                       nextFinishButtonString, canAdvance, showCancel) {
    this.wiz.canAdvance = canAdvance;

    var bnf = this.wiz.getButton(this.wiz.onLastPage ? "finish" : "next");
    var be1 = this.wiz.getButton("extra1");
    var be2 = this.wiz.getButton("extra2");
    var bc = this.wiz.getButton("cancel");

    // Set the labels for the next / finish, extra1, and extra2 buttons
    this._setButton(bnf, nextFinishButtonString);
    this._setButton(be1, extra1ButtonString);
    this._setButton(be2, extra2ButtonString);

    bnf.hidden = bnf.disabled = !nextFinishButtonString;
    be1.hidden = be1.disabled = !extra1ButtonString;
    be2.hidden = be2.disabled = !extra2ButtonString;
    bc.hidden = bc.disabled = !showCancel;

    // Hide and disable the back button each time setButtons is called
    // (see bug 464765).
    var btn = this.wiz.getButton("back");
    btn.hidden = btn.disabled = true;

    // Hide and disable the finish button if not on the last page or the next
    // button if on the last page each time setButtons is called.
    btn = this.wiz.getButton(this.wiz.onLastPage ? "next" : "finish");
    btn.hidden = btn.disabled = true;
  },

  getAUSString: function(key, strings) {
    if (strings)
      return this.strings.getFormattedString(key, strings);
    return this.strings.getString(key);
  },

  never: function () {
    // If the user clicks "No Thanks", we should not prompt them to update to
    // this version again unless they manually select "Check for Updates..."
    // which will clear all of the "never" prefs.
    var neverPrefName = PREF_APP_UPDATE_NEVER_BRANCH + this.update.appVersion;
    Services.prefs.setBoolPref(neverPrefName, true);
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
    this._runUnload = false;
    var pageid = document.documentElement.currentPage.pageid;
    if ("onWizardFinish" in this._pages[pageid])
      this._pages[pageid].onWizardFinish();
  },

  /**
   * Called when the user presses the "Cancel" button on the wizard, dispatches
   * the function call to the selected page.
   */
  onWizardCancel: function() {
    this._runUnload = false;
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

    gLogEnabled = getPref("getBoolPref", PREF_APP_UPDATE_LOG, false)

    this.strings = document.getElementById("updateStrings");
    var brandStrings = document.getElementById("brandStrings");
    this.brandName = brandStrings.getString("brandShortName");

    var pages = this.wiz.childNodes;
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

    // Advance to the Start page.
    this.getStartPageID(function(startPageID) {
      LOG("gUpdates", "onLoad - setting current page to startpage " + startPageID);
      gUpdates.wiz.currentPage = document.getElementById(startPageID);
    });
  },

  /**
   * Called when the wizard UI is unloaded.
   */
  onUnload: function() {
    if (this._runUnload) {
      var cp = this.wiz.currentPage;
      if (cp.pageid != "finished" && cp.pageid != "finishedBackground")
        this.onWizardCancel();
    }
  },

  /**
   * Gets the ID of the <wizardpage> object that should be displayed first. This
   * is an asynchronous method that passes the resulting object to a callback
   * function.
   *
   * This is determined by how we were called by the update prompt:
   *
   * Prompt Method:       Arg0:         Update State: Src Event:  Failed:   Result:
   * showUpdateAvailable  nsIUpdate obj --            background  --        see Note below
   * showUpdateDownloaded nsIUpdate obj pending       background  --        finishedBackground
   * showUpdateInstalled  "installed"   --            --          --        installed
   * showUpdateError      nsIUpdate obj failed        either      partial   errorpatching
   * showUpdateError      nsIUpdate obj failed        either      complete  errors
   * checkForUpdates      null          --            foreground  --        checking
   * checkForUpdates      null          downloading   foreground  --        downloading
   *
   * Note: the page returned (e.g. Result) for showUpdateAvaulable is as follows:
   * New enabled incompatible add-ons   : incompatibleCheck page
   * No new enabled incompatible add-ons: either updatesfoundbasic or
   *                                      updatesfoundbillboard as determined by
   *                                      updatesFoundPageId
   * @param   aCallback
   *          A callback to pass the <wizardpage> object to be displayed first to.
   */
  getStartPageID: function(aCallback) {
    if ("arguments" in window && window.arguments[0]) {
      var arg0 = window.arguments[0];
      if (arg0 instanceof CoI.nsIUpdate) {
        // If the first argument is a nsIUpdate object, we are notifying the
        // user that the background checking found an update that requires
        // their permission to install, and it's ready for download.
        this.setUpdate(arg0);
        if (this.update.errorCode == CERT_ATTR_CHECK_FAILED_NO_UPDATE ||
            this.update.errorCode == CERT_ATTR_CHECK_FAILED_HAS_UPDATE ||
            this.update.errorCode == BACKGROUNDCHECK_MULTIPLE_FAILURES) {
          aCallback("errorextra");
          return;
        }

        var p = this.update.selectedPatch;
        if (p) {
          var state = p.state;
          var patchFailed;
          try {
            patchFailed = this.update.getProperty("patchingFailed");
          }
          catch (e) {
          }
          if (patchFailed) {
            if (patchFailed == "partial" && this.update.patchCount == 2) {
              // If the system failed to apply the partial patch, show the
              // screen which best describes this condition, which is triggered
              // by the |STATE_FAILED| state.
              state = STATE_FAILED;
            }
            else {
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
          case STATE_PENDING_SVC:
          case STATE_APPLIED:
          case STATE_APPLIED_SVC:
            this.sourceEvent = SRCEVT_BACKGROUND;
            aCallback("finishedBackground");
            return;
          case STATE_DOWNLOADING:
            aCallback("downloading");
            return;
          case STATE_FAILED:
            window.getAttention();
            aCallback("errorpatching");
            return;
          case STATE_DOWNLOAD_FAILED:
          case STATE_APPLYING:
            aCallback("errors");
            return;
          }
        }
        if (this.update.licenseURL)
          this.wiz.getPageById(this.updatesFoundPageId).setAttribute("next", "license");

        var self = this;
        this.getShouldCheckAddonCompatibility(function(shouldCheck) {
          if (shouldCheck) {
            var incompatCheckPage = document.getElementById("incompatibleCheck");
            incompatCheckPage.setAttribute("next", self.updatesFoundPageId);
            aCallback(incompatCheckPage.id);
          }
          else {
            aCallback(self.updatesFoundPageId);
          }
        });
        return;
      }
      else if (arg0 == "installed") {
        aCallback("installed");
        return;
      }
    }
    else {
      var um = CoC["@mozilla.org/updates/update-manager;1"].
               getService(CoI.nsIUpdateManager);
      if (um.activeUpdate) {
        this.setUpdate(um.activeUpdate);
        aCallback("downloading");
        return;
      }
    }

    // Provide the ability to test the billboard html
    var billboardTestURL = getPref("getCharPref", PREF_APP_UPDATE_BILLBOARD_TEST_URL, null);
    if (billboardTestURL) {
      var updatesFoundBillboardPage = document.getElementById("updatesfoundbillboard");
      updatesFoundBillboardPage.setAttribute("next", "dummy");
      gUpdatesFoundBillboardPage.onExtra1 = function(){ gUpdates.wiz.cancel(); };
      gUpdatesFoundBillboardPage.onExtra2 = function(){ gUpdates.wiz.cancel(); };
      this.onWizardNext = function() { gUpdates.wiz.cancel(); };
      this.update = { billboardURL        : billboardTestURL,
                      brandName           : this.brandName,
                      displayVersion      : "Billboard Test 1.0",
                      showNeverForVersion : true,
                      type                : "major" };
      aCallback(updatesFoundBillboardPage.id);
    }
    else {
      aCallback("checking");
    }
  },

  getShouldCheckAddonCompatibility: function(aCallback) {
    // this early return should never happen
    if (!this.update) {
      aCallback(false);
      return;
    }

    if (!this.update.appVersion ||
        Services.vc.compare(this.update.appVersion, Services.appinfo.version) == 0) {
      aCallback(false);
      return;
    }

    try {
      var hotfixID = Services.prefs.getCharPref(PREF_EM_HOTFIX_ID);
    }
    catch (e) { }

    var self = this;
    AddonManager.getAllAddons(function(addons) {
      self.addons = [];
      addons.forEach(function(addon) {
        // Protect against code that overrides the add-ons manager and doesn't
        // implement the isCompatibleWith or the findUpdates method.
        if (!("isCompatibleWith" in addon) || !("findUpdates" in addon)) {
          let errMsg = "Add-on doesn't implement either the isCompatibleWith " +
                       "or the findUpdates method!";
          if (addon.id)
            errMsg += " Add-on ID: " + addon.id;
          Components.utils.reportError(errMsg);
          return;
        }

        // If an add-on isn't appDisabled and isn't userDisabled then it is
        // either active now or the user expects it to be active after the
        // restart. If that is the case and the add-on is not installed by the
        // application and is not compatible with the new application version
        // then the user should be warned that the add-on will become
        // incompatible. If an addon's type equals plugin it is skipped since
        // checking plugins compatibility information isn't supported and
        // getting the scope property of a plugin breaks in some environments
        // (see bug 566787). The hotfix add-on is also ignored as it shouldn't
        // block the user from upgrading.
        try {
          if (addon.type != "plugin" && addon.id != hotfixID &&
              !addon.appDisabled && !addon.userDisabled &&
              addon.scope != AddonManager.SCOPE_APPLICATION &&
              addon.isCompatible &&
              !addon.isCompatibleWith(self.update.appVersion,
                                      self.update.platformVersion))
            self.addons.push(addon);
        }
        catch (e) {
          Components.utils.reportError(e);
        }
      });

      aCallback(self.addons.length != 0);
    });
  },

  /**
   * Returns the string page ID for the appropriate updates found page based
   * on the update's metadata.
   */
  get updatesFoundPageId() {
    if (gUpdatesFoundPageId)
      return gUpdatesFoundPageId;
    return gUpdatesFoundPageId = this.update.billboardURL ? "updatesfoundbillboard"
                                                          : "updatesfoundbasic";
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
    gUpdates.setButtons(null, null, null, false, true);
    gUpdates.wiz.getButton("cancel").focus();

    // Clear all of the "never" prefs to handle the scenario where the user
    // clicked "never" for an update, selected "Check for Updates...", and
    // then canceled.  If we don't clear the "never" prefs future
    // notifications will never happen.
    Services.prefs.deleteBranch(PREF_APP_UPDATE_NEVER_BRANCH);

    // The user will be notified if there is an error so clear the background
    // check error count.
    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS))
      Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDERRORS);

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
      pm.mode = "normal";
      pm.value = Math.floor(100 * (position / totalSize));
    },

    /**
     * See nsIUpdateCheckListener
     */
    onCheckComplete: function(request, updates, updateCount) {
      var aus = CoC["@mozilla.org/updates/update-service;1"].
                getService(CoI.nsIApplicationUpdateService);
      gUpdates.setUpdate(aus.selectUpdate(updates, updates.length));
      if (gUpdates.update) {
        LOG("gCheckingPage", "onCheckComplete - update found");
        if (!aus.canApplyUpdates) {
          // Prevent multiple notifications for the same update when the user is
          // unable to apply updates.
          gUpdates.never();
          gUpdates.wiz.goTo("manualUpdate");
          return;
        }

        if (gUpdates.update.licenseURL) {
          // gUpdates.updatesFoundPageId returns the pageid and not the
          // element's id so use the wizard's getPageById method.
          gUpdates.wiz.getPageById(gUpdates.updatesFoundPageId).setAttribute("next", "license");
        }

        gUpdates.getShouldCheckAddonCompatibility(function(shouldCheck) {
          if (shouldCheck) {
            var incompatCheckPage = document.getElementById("incompatibleCheck");
            incompatCheckPage.setAttribute("next", gUpdates.updatesFoundPageId);
            gUpdates.wiz.goTo("incompatibleCheck");
          }
          else {
            gUpdates.wiz.goTo(gUpdates.updatesFoundPageId);
          }
        });
        return;
      }

      LOG("gCheckingPage", "onCheckComplete - no update found");
      gUpdates.wiz.goTo("noupdatesfound");
    },

    /**
     * See nsIUpdateCheckListener
     */
    onError: function(request, update) {
      LOG("gCheckingPage", "onError - proceeding to error page");
      gUpdates.setUpdate(update);
      if (update.errorCode &&
          (update.errorCode == CERT_ATTR_CHECK_FAILED_NO_UPDATE ||
           update.errorCode == CERT_ATTR_CHECK_FAILED_HAS_UPDATE)) {
        gUpdates.wiz.goTo("errorextra");
      }
      else {
        gUpdates.wiz.goTo("errors");
      }
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
 * The "You have outdated plugins" page
 */
var gPluginsPage = {
  /**
   * URL of the plugin updates page
   */
  _url: null,
  
  /**
   * Initialize
   */
  onPageShow: function() {
    var prefs = Services.prefs;
    if (prefs.getPrefType(PREF_PLUGINS_UPDATEURL) == prefs.PREF_INVALID) {
      gUpdates.wiz.goTo("noupdatesfound");
      return;
    }
    
    this._url = Services.urlFormatter.formatURLPref(PREF_PLUGINS_UPDATEURL);
    var link = document.getElementById("pluginupdateslink");
    link.setAttribute("href", this._url);


    var phs = CoC["@mozilla.org/plugin/host;1"].
                 getService(CoI.nsIPluginHost);
    var plugins = phs.getPluginTags();
    var blocklist = CoC["@mozilla.org/extensions/blocklist;1"].
                      getService(CoI.nsIBlocklistService);

    var hasOutdated = false;
    for (let i = 0; i < plugins.length; i++) {
      let pluginState = blocklist.getPluginBlocklistState(plugins[i]);
      if (pluginState == CoI.nsIBlocklistService.STATE_OUTDATED) {
        hasOutdated = true;
        break;
      }
    }
    if (!hasOutdated) {
      gUpdates.wiz.goTo("noupdatesfound");
      return;
    }

    gUpdates.setButtons(null, null, "okButton", true);
    gUpdates.wiz.getButton("finish").focus();
  },
  
  /**
   * Finish button clicked.
   */
  onWizardFinish: function() {
    openURL(this._url);
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

    if (getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true))
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
    LOG("gIncompatibleCheckPage", "onPageShow - checking for updates to " +
        "incompatible add-ons");

    gUpdates.setButtons(null, null, null, false, true);
    gUpdates.wiz.getButton("cancel").focus();
    this._pBar = document.getElementById("incompatibleCheckProgress");
    this._totalCount = gUpdates.addons.length;

    this._pBar.mode = "normal";
    gUpdates.addons.forEach(function(addon) {
      addon.findUpdates(this, AddonManager.UPDATE_WHEN_NEW_APP_DETECTED,
                        gUpdates.update.appVersion,
                        gUpdates.update.platformVersion);
    }, this);
  },

  // Addon UpdateListener
  onCompatibilityUpdateAvailable: function(addon) {
    // Remove the add-on from the list of add-ons that will become incompatible
    // with the new version of the application.
    for (var i = 0; i < gUpdates.addons.length; ++i) {
      if (gUpdates.addons[i].id == addon.id) {
        LOG("gIncompatibleCheckPage", "onCompatibilityUpdateAvailable - " +
            "found update for add-on ID: " + addon.id);
        gUpdates.addons.splice(i, 1);
        break;
      }
    }
  },

  onUpdateAvailable: function(addon, install) {
    // If the new version of this add-on is blocklisted for the new application
    // then it isn't a valid update and the user should still be warned that
    // the add-on will become incompatible.
    let bs = CoC["@mozilla.org/extensions/blocklist;1"].
             getService(CoI.nsIBlocklistService);
    if (bs.isAddonBlocklisted(addon.id, install.version,
                              gUpdates.update.appVersion,
                              gUpdates.update.platformVersion))
      return;

    // Compatibility or new version updates mean the same thing here.
    this.onCompatibilityUpdateAvailable(addon);
  },

  onUpdateFinished: function(addon) {
    ++this._completedCount;
    this._pBar.value = Math.ceil((this._completedCount / this._totalCount) * 100);

    if (this._completedCount < this._totalCount)
      return;

    if (gUpdates.addons.length == 0) {
      LOG("gIncompatibleCheckPage", "onUpdateFinished - updates were found " +
          "for all incompatible add-ons");
    }
    else {
      LOG("gIncompatibleCheckPage", "onUpdateFinished - there are still " +
          "incompatible add-ons");
      if (gUpdates.update.licenseURL) {
        document.getElementById("license").setAttribute("next", "incompatibleList");
      }
      else {
        // gUpdates.updatesFoundPageId returns the pageid and not the element's
        // id so use the wizard's getPageById method.
        gUpdates.wiz.getPageById(gUpdates.updatesFoundPageId).setAttribute("next", "incompatibleList");
      }
    }
    gUpdates.wiz.goTo(gUpdates.updatesFoundPageId);
  }
};

/**
 * The "Unable to Update" page. Provides the user information about why they
 * were unable to update and a manual download url.
 */
var gManualUpdatePage = {
  onPageShow: function() {
    var manualURL = Services.urlFormatter.formatURLPref(PREF_APP_UPDATE_MANUAL_URL);
    var manualUpdateLinkLabel = document.getElementById("manualUpdateLinkLabel");
    manualUpdateLinkLabel.value = manualURL;
    manualUpdateLinkLabel.setAttribute("url", manualURL);

    gUpdates.setButtons(null, null, "okButton", true);
    gUpdates.wiz.getButton("finish").focus();
  }
};

/**
 * The "Updates Are Available" page. Provides the user information about the
 * available update.
 */
var gUpdatesFoundBasicPage = {
  /**
   * Initialize
   */
  onPageShow: function() {
    gUpdates.wiz.canRewind = false;
    var update = gUpdates.update;
    gUpdates.setButtons("askLaterButton",
                        update.showNeverForVersion ? "noThanksButton" : null,
                        "updateButton_" + update.type, true);
    var btn = gUpdates.wiz.getButton("next");
    btn.focus();

    var updateName = update.name;
    if (update.channel == "nightly") {
      updateName = gUpdates.getAUSString("updateNightlyName",
                                         [gUpdates.brandName,
                                          update.displayVersion,
                                          update.buildID]);
    }
    var updateNameElement = document.getElementById("updateName");
    updateNameElement.value = updateName;

    var introText = gUpdates.getAUSString("intro_" + update.type,
                                          [gUpdates.brandName, update.displayVersion]);
    var introElem = document.getElementById("updatesFoundInto");
    introElem.setAttribute("severity", update.type);
    introElem.textContent = introText;

    var updateMoreInfoURL = document.getElementById("updateMoreInfoURL");
    if (update.detailsURL)
      updateMoreInfoURL.setAttribute("url", update.detailsURL);
    else
      updateMoreInfoURL.hidden = true;

    var updateTitle = gUpdates.getAUSString("updatesfound_" + update.type +
                                            ".title");
    document.getElementById("updatesFoundBasicHeader").setAttribute("label", updateTitle);
  },

  onExtra1: function() {
    gUpdates.wiz.cancel();
  },

  onExtra2: function() {
    gUpdates.never();
    gUpdates.wiz.cancel();
  }
};

/**
 * The "Updates Are Available" page with a billboard. Provides the user
 * information about the available update.
 */
var gUpdatesFoundBillboardPage = {
  /**
   * If this page has been previously loaded
   */
  _billboardLoaded: false,

  /**
   * Initialize
   */
  onPageShow: function() {
    var update = gUpdates.update;
    gUpdates.setButtons("askLaterButton",
                        update.showNeverForVersion ? "noThanksButton" : null,
                        "updateButton_" + update.type, true);
    gUpdates.wiz.getButton("next").focus();

    if (this._billboardLoaded)
      return;

    var remoteContent = document.getElementById("updateMoreInfoContent");
    remoteContent.addEventListener("load",
                                   gUpdatesFoundBillboardPage.onBillboardLoad,
                                   false);
    // update_name and update_version need to be set before url
    // so that when attempting to download the url, we can show
    // the formatted "Download..." string
    remoteContent.update_name = gUpdates.brandName;
    remoteContent.update_version = update.displayVersion;

    var billboardTestURL = getPref("getCharPref", PREF_APP_UPDATE_BILLBOARD_TEST_URL, null);
    if (billboardTestURL) {
      // Allow file urls when testing the billboard and fallback to the
      // normal method if the URL isn't a file.
      var scheme = Services.io.newURI(billboardTestURL, null, null).scheme;
      if (scheme == "file")
        remoteContent.testFileUrl = update.billboardURL;
      else
        remoteContent.url = update.billboardURL;
    }
    else
      remoteContent.url = update.billboardURL;

    this._billboardLoaded = true;
  },

  /**
   * When the billboard document has loaded
   */
  onBillboardLoad: function(aEvent) {
    var remoteContent = document.getElementById("updateMoreInfoContent");
    // Note: may be called multiple times due to multiple onLoad events.
    var state = remoteContent.getAttribute("state");
    if (state == "loading" || aEvent.originalTarget != remoteContent)
      return;

    remoteContent.removeEventListener("load", gUpdatesFoundBillboardPage.onBillboardLoad, false);
    if (state == "error") {
      gUpdatesFoundPageId = "updatesfoundbasic";
      var next = gUpdates.wiz.getPageById("updatesfoundbillboard").getAttribute("next");
      gUpdates.wiz.getPageById(gUpdates.updatesFoundPageId).setAttribute("next", next);
      gUpdates.wiz.goTo(gUpdates.updatesFoundPageId);
    }
  },

  onExtra1: function() {
    this.onWizardCancel();
    gUpdates.wiz.cancel();
  },

  onExtra2: function() {
    this.onWizardCancel();
    gUpdates.never();
    gUpdates.wiz.cancel();
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
      LOG("gUpdatesFoundBillboardPage", "onWizardCancel - " +
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
      licenseGroup.focus();
      return;
    }

    gUpdates.wiz.canAdvance = false;

    // Disable the license radiogroup until the EULA has been downloaded
    document.getElementById("acceptDeclineLicense").disabled = true;
    gUpdates.update.setProperty("licenseAccepted", "false");

    licenseContent.addEventListener("load", gLicensePage.onLicenseLoad, false);
    // The update_name and update_version need to be set before url so the ui
    // can display the formatted "Download..." string when attempting to
    // download the url.
    licenseContent.update_name = gUpdates.brandName;
    licenseContent.update_version = gUpdates.update.displayVersion;
    licenseContent.url = gUpdates.update.licenseURL;
  },

  /**
   * When the license document has loaded
   */
  onLicenseLoad: function(aEvent) {
    var licenseContent = document.getElementById("licenseContent");
    // Disable or enable the radiogroup based on the state attribute of
    // licenseContent.
    // Note: may be called multiple times due to multiple onLoad events.
    var state = licenseContent.getAttribute("state");
    if (state == "loading" || aEvent.originalTarget != licenseContent)
      return;

    licenseContent.removeEventListener("load", gLicensePage.onLicenseLoad, false);

    if (state == "error") {
      gUpdates.wiz.goTo("manualUpdate");
      return;
    }

    gLicensePage._licenseLoaded = true;
    document.getElementById("acceptDeclineLicense").disabled = false;
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

  /**
   * The non-standard "Back" button.
   */
  onExtra1: function() {
    gUpdates.wiz.goTo(gUpdates.updatesFoundPageId);
  },

  /**
   * When the user clicks next after accepting the license
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
    var listbox = document.getElementById("incompatibleListbox");
    if (listbox.children.length > 0)
      return;

    var intro = gUpdates.getAUSString("incompatAddons_" + gUpdates.update.type,
                                      [gUpdates.brandName,
                                       gUpdates.update.displayVersion]);
    document.getElementById("incompatibleListDesc").textContent = intro;

    var addons = gUpdates.addons;
    for (var i = 0; i < addons.length; ++i) {
      var listitem = document.createElement("listitem");
      var addonLabel = gUpdates.getAUSString("addonLabel", [addons[i].name,
                                                            addons[i].version]);
      listitem.setAttribute("label", addonLabel);
      listbox.appendChild(listitem);
    }
  },

  /**
   * The non-standard "Back" button.
   */
  onExtra1: function() {
    gUpdates.wiz.goTo(gUpdates.update.licenseURL ? "license"
                                                 : gUpdates.updatesFoundPageId);
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
  _downloadStatus: null,
  _downloadProgress: null,
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
  _startTime: null,
  _pausedStatus: "",

  _hiding: false,

  /**
   * Have we registered an observer for a background update being staged
   */
  _updateApplyingObserver: false,

  /**
   * Initialize
   */
  onPageShow: function() {
    this._downloadStatus = document.getElementById("downloadStatus");
    this._downloadProgress = document.getElementById("downloadProgress");
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

    this._startTime = Date.now();

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
        this.cleanUp();
        gUpdates.wiz.goTo("errors");
        return;
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
      if (this._downloadProgress.mode != "normal")
        this._downloadProgress.mode = "normal";
      this._pauseButton.setAttribute("tooltiptext",
                                     gUpdates.getAUSString("pauseButtonResume"));
      this._pauseButton.setAttribute("paused", "true");
      var p = u.selectedPatch.QueryInterface(CoI.nsIPropertyBag);
      var status = p.getProperty("status");
      if (status) {
        let pausedStatus = gUpdates.getAUSString("downloadPausedStatus", [status]);
        this._setStatus(pausedStatus);
      }
    }
    else {
      if (this._downloadProgress.mode != "undetermined")
        this._downloadProgress.mode = "undetermined";
      this._pauseButton.setAttribute("paused", "false");
      this._pauseButton.setAttribute("tooltiptext",
                                     gUpdates.getAUSString("pauseButtonPause"));
      this._setStatus(this._label_downloadStatus);
    }
  },

  /**
   * Wait for an update being staged in the background.
   */
  _setUpdateApplying: function() {
    this._downloadProgress.mode = "undetermined";
    this._pauseButton.hidden = true;
    let applyingStatus = gUpdates.getAUSString("applyingUpdate");
    this._setStatus(applyingStatus);

    Services.obs.addObserver(this, "update-staged", false);
    this._updateApplyingObserver = true;
  },

  /**
   * Clean up the listener and observer registered for the wizard.
   */
  cleanUp: function() {
    var aus = CoC["@mozilla.org/updates/update-service;1"].
              getService(CoI.nsIApplicationUpdateService);
    aus.removeDownloadListener(this);

    if (this._updateApplyingObserver) {
      Services.obs.removeObserver(this, "update-staged");
      this._updateApplyingObserver = false;
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
   * When the user has closed the window using a Window Manager control (this
   * page doesn't have a cancel button) cancel the update in progress.
   */
  onWizardCancel: function() {
    if (this._hiding)
      return;

    this.cleanUp();
  },

  /**
   * When the user closes the Wizard UI by clicking the Hide button
   */
  onHide: function() {
    // Set _hiding to true to prevent onWizardCancel from cancelling the update
    // that is in progress.
    this._hiding = true;

    // Remove ourself as a download listener so that we don't continue to be
    // fed progress and state notifications after the UI we're updating has
    // gone away.
    this.cleanUp();

    var aus = CoC["@mozilla.org/updates/update-service;1"].
              getService(CoI.nsIApplicationUpdateService);
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
      var ps = Services.prompt;
      var flags = ps.STD_YES_NO_BUTTONS;
      // Focus the software update wizard before prompting. This will raise
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
    // This !paused test is necessary because onStartRequest may fire after
    // the download was paused (for those speedy clickers...)
    if (this._paused)
      return;

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

    if (this._downloadProgress.mode != "normal")
      this._downloadProgress.mode = "normal";
    if (this._downloadProgress.value != currentProgress)
      this._downloadProgress.value = currentProgress;
    if (this._pauseButton.disabled)
      this._pauseButton.disabled = false;

    // If the update has completed downloading and the download status contains
    // the original text return early to avoid an assertion in debug builds.
    // Since the page will advance immmediately due to the update completing the
    // download updating the status is not important.
    // nsTextFrame::GetTrimmedOffsets 'Can only call this on frames that have
    // been reflowed'.
    if (progress == maxProgress &&
        this._downloadStatus.textContent == this._label_downloadStatus)
      return;

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
    if (this._downloadProgress.mode != "normal")
      this._downloadProgress.mode = "normal";

    var u = gUpdates.update;
    switch (status) {
    case CoR.NS_ERROR_UNEXPECTED:
      if (u.selectedPatch.state == STATE_DOWNLOAD_FAILED &&
          (u.isCompleteUpdate || u.patchCount != 2)) {
        // Verification error of complete patch, informational text is held in
        // the update object.
        this.cleanUp();
        gUpdates.wiz.goTo("errors");
        break;
      }
      // Verification failed for a partial patch, complete patch is now
      // downloading so return early and do NOT remove the download listener!

      // Reset the progress meter to "undertermined" mode so that we don't
      // show old progress for the new download of the "complete" patch.
      this._downloadProgress.mode = "undetermined";
      this._pauseButton.disabled = true;
      document.getElementById("verificationFailed").hidden = false;
      break;
    case CoR.NS_BINDING_ABORTED:
      LOG("gDownloadingPage", "onStopRequest - pausing download");
      // Do not remove UI listener since the user may resume downloading again.
      break;
    case CoR.NS_OK:
      LOG("gDownloadingPage", "onStopRequest - patch verification succeeded");
      // If the background update pref is set, we should wait until the update
      // is actually staged in the background.
      var aus = CoC["@mozilla.org/updates/update-service;1"].
                getService(CoI.nsIApplicationUpdateService);
      if (aus.canStageUpdates) {
        this._setUpdateApplying();
      } else {
        this.cleanUp();
        gUpdates.wiz.goTo("finished");
      }
      break;
    default:
      LOG("gDownloadingPage", "onStopRequest - transfer failed");
      // Some kind of transfer error, die.
      this.cleanUp();
      gUpdates.wiz.goTo("errors");
      break;
    }
  },

  /**
   * See nsIObserver.idl
   */
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "update-staged") {
      if (aData == STATE_DOWNLOADING) {
        // We've fallen back to downloding the full update because the
        // partial update failed to get staged in the background.
        this._setStatus("downloading");
        return;
      }
      this.cleanUp();
      if (aData == STATE_APPLIED ||
          aData == STATE_APPLIED_SVC ||
          aData == STATE_PENDING ||
          aData == STATE_PENDING_SVC) {
        // If the update is successfully applied, or if the updater has
        // fallen back to non-staged updates, go to the finish page.
        gUpdates.wiz.goTo("finished");
      } else {
        gUpdates.wiz.goTo("errors");
      }
    }
  },

  /**
   * See nsISupports.idl
   */
  QueryInterface: function(iid) {
    if (!iid.equals(CoI.nsIRequestObserver) &&
        !iid.equals(CoI.nsIProgressEventSink) &&
        !iid.equals(CoI.nsIObserver) &&
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
    gUpdates.setButtons(null, null, "okButton", true);
    gUpdates.wiz.getButton("finish").focus();

    var statusText = gUpdates.update.statusText;
    LOG("gErrorsPage" , "onPageShow - update.statusText: " + statusText);

    var errorReason = document.getElementById("errorReason");
    errorReason.value = statusText;
    var manualURL = Services.urlFormatter.formatURLPref(PREF_APP_UPDATE_MANUAL_URL);
    var errorLinkLabel = document.getElementById("errorLinkLabel");
    errorLinkLabel.value = manualURL;
    errorLinkLabel.setAttribute("url", manualURL);
  }
};

/**
 * The page shown when there is a background check or a certificate attribute
 * error.
 */
var gErrorExtraPage = {
  /**
   * Initialize
   */
  onPageShow: function() {
    gUpdates.setButtons(null, null, "okButton", true);
    gUpdates.wiz.getButton("finish").focus();

    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CERT_ERRORS))
      Services.prefs.clearUserPref(PREF_APP_UPDATE_CERT_ERRORS);

    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS))
      Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDERRORS);

    if (gUpdates.update.errorCode == CERT_ATTR_CHECK_FAILED_HAS_UPDATE) {
      document.getElementById("errorCertAttrHasUpdateLabel").hidden = false;
    }
    else {
      if (gUpdates.update.errorCode == CERT_ATTR_CHECK_FAILED_NO_UPDATE)
        document.getElementById("errorCertCheckNoUpdateLabel").hidden = false;
      else
        document.getElementById("genericBackgroundErrorLabel").hidden = false;
      var manualURL = Services.urlFormatter.formatURLPref(PREF_APP_UPDATE_MANUAL_URL);
      var errorLinkLabel = document.getElementById("errorExtraLinkLabel");
      errorLinkLabel.value = manualURL;
      errorLinkLabel.setAttribute("url", manualURL);
      errorLinkLabel.hidden = false;
    }
  }
};

/**
 * The "There was an error applying a partial patch" page.
 */
var gErrorPatchingPage = {
  /**
   * Initialize
   */
  onPageShow: function() {
    gUpdates.setButtons(null, null, "okButton", true);
  },

  onWizardNext: function() {
    switch (gUpdates.update.selectedPatch.state) {
    case STATE_PENDING:
    case STATE_PENDING_SVC: 
      gUpdates.wiz.goTo("finished");
      break;
    case STATE_DOWNLOADING:
      gUpdates.wiz.goTo("downloading");
      break;
    case STATE_DOWNLOAD_FAILED:
      gUpdates.wiz.goTo("errors");
      break;
    }
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
    gUpdates.wiz.getButton("finish").focus();
  },

  /**
   * Initialize the Wizard Page for a Background Source Event
   */
  onPageShowBackground: function() {
    this.onPageShow();
    var updateFinishedName = document.getElementById("updateFinishedName");
    updateFinishedName.value = gUpdates.update.name;

    var link = document.getElementById("finishedBackgroundLink");
    if (gUpdates.update.detailsURL) {
      link.setAttribute("url", gUpdates.update.detailsURL);
      // The details link is stealing focus so it is disabled by default and
      // should only be enabled after onPageShow has been called.
      link.disabled = false;
    }
    else
      link.hidden = true;

    if (getPref("getBoolPref", PREF_APP_UPDATE_TEST_LOOP, false)) {
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
    var cancelQuit = CoC["@mozilla.org/supports-PRBool;1"].
                     createInstance(CoI.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested",
                                 "restart");

    // Something aborted the quit process.
    if (cancelQuit.data)
      return;

    // If already in safe mode restart in safe mode (bug 327119)
    if (Services.appinfo.inSafeMode) {
      let env = CoC["@mozilla.org/process/environment;1"].
                getService(CoI.nsIEnvironment);
      env.set("MOZ_SAFE_MODE_RESTART", "1");
    }

    // Restart the application
    CoC["@mozilla.org/toolkit/app-startup;1"].getService(CoI.nsIAppStartup).
    quit(CoI.nsIAppStartup.eAttemptQuit | CoI.nsIAppStartup.eRestart);
  },

  /**
   * When the user clicks the "Restart Later" instead of the Restart Now" button
   * in the wizard after an update has been downloaded.
   */
  onExtra1: function() {
    // XXXrstrong - reminding the user to restart is broken (see bug 464835)
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
    var branding = document.getElementById("brandStrings");
    try {
      // whatsNewURL should just be a pref (bug 546609).
      var url = branding.getFormattedString("whatsNewURL", [Services.appinfo.version]);
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
 * Callback for the Update Prompt to set the current page if an Update Wizard
 * window is already found to be open.
 * @param   pageid
 *          The ID of the page to switch to
 */
function setCurrentPage(pageid) {
  gUpdates.wiz.currentPage = document.getElementById(pageid);
}
