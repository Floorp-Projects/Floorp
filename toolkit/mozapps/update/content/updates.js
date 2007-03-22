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

const nsIUpdateItem           = Components.interfaces.nsIUpdateItem;
const nsIIncrementalDownload  = Components.interfaces.nsIIncrementalDownload;

const XMLNS_XUL               = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const PREF_UPDATE_MANUAL_URL        = "app.update.url.manual";
const PREF_UPDATE_NAGTIMER_DL       = "app.update.nagTimer.download";
const PREF_UPDATE_NAGTIMER_RESTART  = "app.update.nagTimer.restart";
const PREF_APP_UPDATE_LOG_BRANCH    = "app.update.log.";
const PREF_UPDATE_TEST_LOOP         = "app.update.test.loop";
const PREF_UPDATE_NEVER_BRANCH      = "app.update.never.";

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
    dump("*** " + module + ":" + string + "\n");
    gConsole.logStringMessage(string);
  }
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
    LOG("General", "Failed to get preference " + preference);
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
      var label = this.strings.getString(string);
      if (label.indexOf("%S") != -1)
        label = label.replace(/%S/, this.brandName);
      button.label = label;
      button.setAttribute("accesskey",
                          this.strings.getString(string + ".accesskey"));
    } else {
      button.label = button.defaultLabel;
      button.setAttribute("accesskey", button.defaultAccesskey);
    }
  },
  
  /**
   * Set the state for the Wizard's control buttons (labels and disabled
   * state).
   * @param   backButtonString
   *          The property in the stringbundle containing the label
   *          to put on the Back button, or null for default.
   * @param   backButtonDisabled
   *          true if the Back button should be disabled, false otherwise
   * @param   nextButtonString
   *          The property in the stringbundle containing the label
   *          to put on the Next button, or null for default.
   * @param   nextButtonDisabled
   *          true if the Next button should be disabled, false otherwise
   * @param   finishButtonString
   *          The property in the stringbundle containing the label
   *          to put on the Finish button, or null for default.
   * @param   finishButtonDisabled
   *          true if the Finish button should be disabled, false otherwise
   * @param   cancelButtonString
   *          The property in the stringbundle containing the label
   *          to put on the Cancel button, or null for default.
   * @param   cancelButtonDisabled
   *          true if the Cancel button should be disabled, false otherwise
   * @param   hideBackAndCancelButtons
   *          true if the Cancel button should be hidden, false otherwise
   * @param   extraButton1String
   *          The property in the stringbundle containing the label to put
   *          on the first Extra button, if present, or null for default.
   * @param   extraButton1Disabled
   *          true if the first Extra button should be disabled, 
   *          false otherwise
   * @param   extraButton2String
   *          The property in the stringbundle containing the label to put
   *          on the second Extra button, if present, or null for default.
   * @param   extraButton2Disabled
   *          true if the second Extra button should be disabled, 
   *          false otherwise
   */
  setButtons: function(backButtonString, backButtonDisabled,
                       nextButtonString, nextButtonDisabled,
                       finishButtonString, finishButtonDisabled,
                       cancelButtonString, cancelButtonDisabled,
                       hideBackAndCancelButtons,
                       extraButton1String, extraButton1Disabled,
                       extraButton2String, extraButton2Disabled) {
    var bb = this.wiz.getButton("back");
    var bn = this.wiz.getButton("next");
    var bf = this.wiz.getButton("finish");
    var bc = this.wiz.getButton("cancel");
    var be1 = this.wiz.getButton("extra1");
    var be2 = this.wiz.getButton("extra2");

    this._setButton(bb, backButtonString);
    this._setButton(bn, nextButtonString);
    this._setButton(bf, finishButtonString);
    this._setButton(bc, cancelButtonString);
    this._setButton(be1, extraButton1String);
    this._setButton(be2, extraButton2String);

    // update button state using the wizard commands
    this.wiz.canRewind  = !backButtonDisabled;
    // The Finish and Next buttons are never exposed at the same time
    if (this.wiz.onLastPage)
      this.wiz.canAdvance = !finishButtonDisabled;
    else
      this.wiz.canAdvance = !nextButtonDisabled;

    bf.disabled = finishButtonDisabled;
    bc.disabled = cancelButtonDisabled;
    be1.disabled = extraButton1Disabled;
    be2.disabled = extraButton2Disabled;

    // Show or hide the cancel and back buttons, since the first Extra 
    // button does this job.
    bc.hidden   = hideBackAndCancelButtons;  
    bb.hidden   = hideBackAndCancelButtons;

    // Show or hide the extra buttons
    be1.hidden = extraButton1String == null;
    be2.hidden = extraButton2String == null;
  },
  
  never: function () {
    // if the user hits "Never", we should not prompt them about this
    // major version again, unless they manually do "Check for Updates..."
    // which, will clear the "never" pref for the version presented
    // so that if they do "Later", we will remind them later.
    //
    // fix for bug #359093
    // version might one day come back from AUS as an 
    // arbitrary (and possibly non ascii) string, so we need to encode it
    var neverPrefName = PREF_UPDATE_NEVER_BRANCH + encodeURIComponent(gUpdates.update.version);
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
    
    gPref = Components.classes["@mozilla.org/preferences-service;1"]
                      .getService(Components.interfaces.nsIPrefBranch2);
    gConsole = Components.classes["@mozilla.org/consoleservice;1"]
                         .getService(Components.interfaces.nsIConsoleService);  
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
    this._cacheButtonStrings("back");
    this._cacheButtonStrings("next");
    this._cacheButtonStrings("finish");
    this._cacheButtonStrings("cancel");
    this._cacheButtonStrings("extra1");
    this._cacheButtonStrings("extra2");
    
    // Advance to the Start page. 
    gUpdates.wiz.currentPage = this.startPage;
  },

  /**
   * Initialize Logging preferences, formatted like so:
   *  app.update.log.<moduleName> = <true|false>
   */
  _initLoggingPrefs: function() {
    try {
      var ps = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefService);
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
   * U'Prompt Method:     Arg0:         Update State: Src Event:  p'Failed: Result:
   * showUpdateAvailable  nsIUpdate obj --            background  --        updatesfound
   * showUpdateDownloaded nsIUpdate obj pending       background  --        finishedBackground
   * showUpdateInstalled  nsIUpdate obj succeeded     either      --        installed
   * showUpdateError      nsIUpdate obj failed        either      partial   errorpatching
   * showUpdateError      nsIUpdate obj failed        either      complete  errors
   * checkForUpdates      null          --            foreground  --        checking
   * checkForUpdates      null          downloading   foreground  --        downloading
   */
  get startPage() {
    if (window.arguments) {
      var arg0 = window.arguments[0];
      if (arg0 instanceof Components.interfaces.nsIUpdate) {
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
          case STATE_SUCCEEDED:
            return document.getElementById("installed");
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
        return document.getElementById("updatesfound");
      }
    }
    else {
      var um = 
          Components.classes["@mozilla.org/updates/update-manager;1"].
          getService(Components.interfaces.nsIUpdateManager);
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
      this.update.QueryInterface(Components.interfaces.nsIWritablePropertyBag);
  },
  
  /**
   * Registers a timer to nag the user about something relating to update
   * @param   timerID
   *          The ID of the timer to register, used for persistence
   * @param   timerInterval
   *          The interval of the timer
   * @param   methodName
   *          The method to call on the Update Prompter when the timer fires
   */
  registerNagTimer: function(timerID, timerInterval, methodName) {
    // Remind the user to restart their browser in a little bit.
    var tm = 
        Components.classes["@mozilla.org/updates/timer-manager;1"].
        getService(Components.interfaces.nsIUpdateTimerManager);
    
    /**
     * An object implementing nsITimerCallback that uses the Update Prompt
     * component to notify the user about some event relating to app update
     * that they should take action on.
     * @param   update
     *          The nsIUpdate object in question
     * @param   methodName
     *          The name of the method on the Update Prompter that should be 
     *          called
     * @constructor
     */
    function Callback(update, methodName) {
      this._update = update;
      this._methodName = methodName;
      this._prompter = 
        Components.classes["@mozilla.org/updates/update-prompt;1"].
        createInstance(Components.interfaces.nsIUpdatePrompt);      
    }
    Callback.prototype = {
      /**
       * The Update we should nag about downloading
       */
      _update: null,
      
      /**
       * The Update prompter we can use to notify the user
       */
      _prompter: null,
      
      /**
       * The method on the update prompt that should be called
       */
      _methodName: "",
      
      /**
       * Called when the timer fires. Notifies the user about whichever event 
       * they need to be nagged about (e.g. update available, please restart,
       * etc).
       */
      notify: function(timerCallback) {
        if (methodName in this._prompter) 
          this._prompter[methodName](null, this._update);
      }
    }
    tm.registerTimer(timerID, (new Callback(gUpdates.update, methodName)), 
                     timerInterval);
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
   * Starts the update check when the page is shown.
   */
  onPageShow: function() {
    gUpdates.setButtons(null, true, null, true, null, true, 
                        null, false, false, null, 
                        false, null, false);
    this._checker = 
      Components.classes["@mozilla.org/updates/update-checker;1"].
      createInstance(Components.interfaces.nsIUpdateChecker);
    this._checker.checkForUpdates(this.updateListener, true);
  },
  
  /**
   * The user has closed the window, either by pressing cancel or using a Window
   * Manager control, so stop checking for updates.
   */
  onWizardCancel: function() {
    if (this._checker) {
      const nsIUpdateChecker = Components.interfaces.nsIUpdateChecker;
      this._checker.stopChecking(nsIUpdateChecker.CURRENT_CHECK);
    }
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
      checkingProgress.setAttribute("value", Math.floor(100 * (position/totalSize)));
    },

    /**
     * See nsIUpdateCheckListener
     */
    onCheckComplete: function(request, updates, updateCount) {
      var aus = Components.classes["@mozilla.org/updates/update-service;1"]
                          .getService(Components.interfaces.nsIApplicationUpdateService);
      gUpdates.setUpdate(aus.selectUpdate(updates, updates.length));
      if (!gUpdates.update) {
        LOG("UI:CheckingPage", 
            "Could not select an appropriate update, either because there " + 
            "were none, or |selectUpdate| failed.");
        var checking = document.getElementById("checking");
        checking.setAttribute("next", "noupdatesfound");
      }
      gUpdates.wiz.canAdvance = true;
      gUpdates.wiz.advance();
    },

    /**
     * See nsIUpdateCheckListener
     */
    onError: function(request, update) {
      LOG("UI:CheckingPage", "UpdateCheckListener: error");

      gUpdates.setUpdate(update);

      gUpdates.wiz.currentPage = document.getElementById("errors");
    },
    
    /**
     * See nsISupports.idl
     */
    QueryInterface: function(aIID) {
      if (!aIID.equals(Components.interfaces.nsIUpdateCheckListener) &&
          !aIID.equals(Components.interfaces.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
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
    gUpdates.setButtons(null, true, null, true, null, false, "hideButton",
                        true, false, null, false, null, false);
    gUpdates.wiz.getButton("finish").focus();
  }
};

/**
 * The "Updates Are Available" page. Provides the user information about the
 * available update, extensions it might make incompatible, and a means to 
 * continue downloading and installing it.
 */
var gUpdatesAvailablePage = {
  /**
   * An array of installed addons incompatible with this update.
   */
  _incompatibleItems: null,

  /**
   * The <license> binding (which we are also using for the details content)
   */
  _updateMoreInfoContent: null,
  
  /**
   * Initialize.
   */
  onPageShow: function() {
    // disable the "next" button, as we don't want the user to
    // be able to progress with the update util we show them the details
    // which might include a warning about incompatible add-ons
    // disable the "back" button, as well (note the call to setButtons()
    // below does this, too).  This just leaves the cancel button
    // until we are ready
    gUpdates.wiz.getButton("next").disabled = true;
    gUpdates.wiz.getButton("back").disabled = true;

    var updateName = gUpdates.strings.getFormattedString("updateName", 
      [gUpdates.brandName, gUpdates.update.version]);
    if (gUpdates.update.channel == "nightly")
      updateName = updateName + " nightly (" + gUpdates.update.buildID + ")";
    var updateNameElement = document.getElementById("updateName");
    updateNameElement.value = updateName;
    var severity = gUpdates.update.type;
    var updateTypeElement = document.getElementById("updateType");
    updateTypeElement.setAttribute("severity", severity);
 
    var intro;
    if (severity == "major") {
      // for major updates, use the brandName and the version for the intro
      intro = gUpdates.strings.getFormattedString(
        "introType_major_app_and_version", 
        [gUpdates.brandName, gUpdates.update.version]);

      this._updateMoreInfoContent = 
        document.getElementById("updateMoreInfoContent");

      // update_name and update_version need to be set before url
      // so that when attempting to download the url, we can show
      // the formatted "Download..." string
      this._updateMoreInfoContent.update_name = gUpdates.brandName;
      this._updateMoreInfoContent.update_version = gUpdates.update.version;
      this._updateMoreInfoContent.url = gUpdates.update.detailsURL;
    }
    else {
      // for minor updates, do not include the version
      // just use the brandName for the intro
      intro = gUpdates.strings.getFormattedString(
        "introType_minor_app", [gUpdates.brandName]);

      var updateMoreInfoURL = document.getElementById("updateMoreInfoURL");
      updateMoreInfoURL.href = gUpdates.update.detailsURL; 
    }

    var updateTitle = gUpdates.strings
                              .getString("updatesfound_" + severity + ".title");
    gUpdates.wiz.currentPage.setAttribute("label", updateTitle);
    // this is necessary to make this change to the label of the current
    // wizard page show up
    gUpdates.wiz._adjustWizardHeader();

    while (updateTypeElement.hasChildNodes())
      updateTypeElement.removeChild(updateTypeElement.firstChild);
    updateTypeElement.appendChild(document.createTextNode(intro));
    
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    var items = em.getIncompatibleItemList("", gUpdates.update.version,
                                           nsIUpdateItem.TYPE_ADDON, false, 
                                           { });
    if (items.length > 0) {
      // There are addons that are incompatible with this update, so show the 
      // warning message.
      var incompatibleWarning = document.getElementById("incompatibleWarning");
      incompatibleWarning.hidden = false;
      
      this._incompatibleItems = items;
    }
    
    // wait to show the additional details until after we do the check
    // for add-on incompatibilty
    this.onShowMoreDetails();

    var licenseAccepted;
    try {
      licenseAccepted = gUpdates.update.getProperty("licenseAccepted");
    }
    catch (e) {
      gUpdates.update.setProperty("licenseAccepted", "false");
      licenseAccepted = false;
    }
    
    // only major updates show EULAs
    if (gUpdates.update.type == "major" &&
        gUpdates.update.licenseURL && !licenseAccepted)
      gUpdates.wiz.currentPage.setAttribute("next", "license");

    gUpdates.setButtons(null, true, "downloadButton_" + severity, 
                        false, null, false,
                        null, false, true, 
                        "laterButton", false, 
                        severity == "major" ? "neverButton" : null, false);
    gUpdates.wiz.getButton("next").focus();
  },
  
  /**
   * User clicked the "More Details..." button
   */
  onShowMoreDetails: function() {
    var updateTypeElement = document.getElementById("updateType");
    var moreInfoURL = document.getElementById("moreInfoURL");
    var moreInfoContent = document.getElementById("moreInfoContent");

    if (updateTypeElement.getAttribute("severity") == "major") {
      moreInfoURL.hidden = true;
      moreInfoContent.hidden = false;
      document.getElementById("updateName").hidden = true;
      document.getElementById("updateNameSep").hidden = true;
      document.getElementById("upgradeEvangelism").hidden = true;
      document.getElementById("upgradeEvangelismSep").hidden = true;

      // clear the "never" pref for this version.  this is to handle the
      // scenario where the user clicked "never" for a major update
      // and then at a later point, did "Check for Updates..." 
      // and then hit "Later".  If we don't clear the "never" pref
      // "Later" will never happen.
      //
      // fix for bug #359093
      // version might one day come back from AUS as an 
      // arbitrary (and possibly non ascii) string, so we need to encode it
      var neverPrefName = PREF_UPDATE_NEVER_BRANCH + encodeURIComponent(gUpdates.update.version);
      gPref.setBoolPref(neverPrefName, false);
    }
    else {
      moreInfoURL.hidden = false;
      moreInfoContent.hidden = true;
    }

    // in order to prevent showing the blank xul:browser (<license> binding)
    // delay setting the selected index of the detailsDeck until after 
    // we've set everything up.  see bug #352400 for more details.
    var detailsDeck = document.getElementById("detailsDeck");
    detailsDeck.selectedIndex = 1;
  },

  /**
   * When the user cancels the wizard or they decline the license
   */
  onWizardCancel: function() {
    try {
      // If the _updateMoreInfoContent was downloading, stop it.
      if (this._updateMoreInfoContent)
        this._updateMoreInfoContent.stopDownloading();
    }
    catch (ex) {
      dump("XXX _updateMoreInfoContent.stopDownloading() failed: " + ex + "\n");
    }
  },

  /** 
   * Show a list of extensions made incompatible by this update.
   */
  showIncompatibleItems: function() {
    openDialog("chrome://mozapps/content/update/incompatible.xul", "", 
               "dialog,centerscreen,modal,resizable,titlebar", this._incompatibleItems);
  }
};

/**
 * The page which shows the user a license associated with an update. The
 * user must agree to the terms of the license before continuing to install
 * the update.
 */
var gLicensePage = {
  /**
   * The <license> element
   */
  _licenseContent: null,

  /**
   * Initialize
   */
  onPageShow: function() {
    this._licenseContent = document.getElementById("licenseContent");
    
    // the license radiogroup is disabled until the EULA is downloaded
    document.getElementById("acceptDeclineLicense").disabled = true;

    gUpdates.setButtons(null, true, null, true, null, true, null, 
                        false, false, null, false, null, false);

    this._licenseContent.addEventListener("load", this.onLicenseLoad, false);
    // update_name and update_version need to be set before url
    // so that when attempting to download the url, we can show
    // the formatted "Download..." string
    this._licenseContent.update_name = gUpdates.brandName;
    this._licenseContent.update_version = gUpdates.update.version;
    this._licenseContent.url = gUpdates.update.licenseURL;
  },
  
  /**
   * When the license document has loaded
   */
  onLicenseLoad: function() {
    // on the load event, disable or enable the radiogroup based on 
    // the state of the licenseContent.  note, you may get multiple
    // onLoad events
    document.getElementById("acceptDeclineLicense").disabled =
      (gLicensePage._licenseContent.getAttribute("state") == "error");
  },

  /**
   * When the user changes the state of the accept / decline radio group
   */
  onAcceptDeclineRadio: function() {
    var selectedIndex = document.getElementById("acceptDeclineLicense")
                                .selectedIndex;
    // 0 == Accept, 1 == Decline
    var licenseAccepted = (selectedIndex == 0);
    gUpdates.wiz.getButton("next").disabled = !licenseAccepted;
    gUpdates.wiz.canAdvance = licenseAccepted;
  },
  
  /**
   * When the user accepts the license by hitting "Next"
   */
  onWizardNext: function() {
    try {
      gUpdates.update.setProperty("licenseAccepted", "true");
      var um = 
        Components.classes["@mozilla.org/updates/update-manager;1"].
        getService(Components.interfaces.nsIUpdateManager);
      um.saveUpdates();
    }
    catch (ex) {
      dump("XXX ex " + ex + "\n");
    }
  },
  
  /**
   * When the user cancels the wizard or they decline the license
   */
  onWizardCancel: function() {
    try {
      // If the license was downloading, stop it.
      if (this._licenseContent)
        this._licenseContent.stopDownloading();
    }
    catch (ex) {
      dump("XXX _licenseContent.stopDownloading() failed: " + ex + "\n");
    }
  }
};

/**
 * Formats status messages for a download operation based on the progress
 * of the download.
 * @constructor
 */
function DownloadStatusFormatter() {
  this._startTime = Math.floor((new Date()).getTime() / 1000);
  this._elapsed = 0;
  
  var us = gUpdates.strings;
  this._statusFormat = us.getString("statusFormat");

  this._progressFormat = us.getString("progressFormat");
  this._progressFormatKBMB = us.getString("progressFormatKBMB");
  this._progressFormatKBKB = us.getString("progressFormatKBKB");
  this._progressFormatMBMB = us.getString("progressFormatMBMB");
  this._progressFormatUnknownMB = us.getString("progressFormatUnknownMB");
  this._progressFormatUnknownKB = us.getString("progressFormatUnknownKB");

  this._rateFormat = us.getString("rateFormat");
  this._rateFormatKBSec = us.getString("rateFormatKBSec");
  this._rateFormatMBSec = us.getString("rateFormatMBSec");

  this._timeFormat = us.getString("timeFormat");
  this._longTimeFormat = us.getString("longTimeFormat");
  this._shortTimeFormat = us.getString("shortTimeFormat");

  this._remain = us.getString("remain");
  this._unknownFilesize = us.getString("unknownFilesize");
}
DownloadStatusFormatter.prototype = {
  /**
   * Time when the download started (in seconds since epoch)
   */
  _startTime: 0,

  /**
   * Time elapsed since the start of the download operation (in seconds)
   */
  _elapsed: -1,
  
  /**
   * Transfer rate of the download
   */
  _rate: 0,
  
  /**
   * Transfer rate of the download, formatted as text
   */
  _rateFormatted: "",
  
  /**
   * Transfer rate, formatted into text container
   */
  _rateFormattedContainer: "",
  
  /**
   * Number of Kilobytes downloaded so far in the form:
   *  376KB of 9.3MB
   */
  progress: "",

  /**
   * Format a human-readable status message based on the current download
   * progress.
   * @param   currSize
   *          The current number of bytes transferred
   * @param   finalSize
   *          The total number of bytes to be transferred
   * @returns A human readable status message, e.g.
   *          "3.4 of 4.7MB; 01:15 remain"
   */
  formatStatus: function(currSize, finalSize) {
    var now = Math.floor((new Date()).getTime() / 1000);
    
    // 1) Determine the Download Progress in Kilobytes
    var total = parseInt(finalSize/1024 + 0.5);
    this.progress = this._formatKBytes(parseInt(currSize/1024 + 0.5), total);
    
    var progress = this._replaceInsert(this._progressFormat, 1, this.progress);
    var rateFormatted = "";
    
    // 2) Determine the Transfer Rate
    var oldElapsed = this._elapsed;
    this._elapsed = now - this._startTime;
    if (oldElapsed != this._elapsed) {
      this._rate = this._elapsed ? Math.floor((currSize / 1024) / this._elapsed) : 0;
      var isKB = true;
      if (parseInt(this._rate / 1024) > 0) {
        this._rate = (this._rate / 1024).toFixed(1);
        isKB = false;
      }
      if (this._rate > 100)
        this._rate = Math.round(this._rate);
      
      if (this._rate) {
        var format = isKB ? this._rateFormatKBSec : this._rateFormatMBSec;
        this._rateFormatted = this._replaceInsert(format, 1, this._rate);
        this._rateFormattedContainer = this._replaceInsert(" " + this._rateFormat, 1, this._rateFormatted);
      }
    }
    progress = this._replaceInsert(progress, 2, this._rateFormattedContainer);
    

    // 3) Determine the Time Remaining
    var remainingTime = "";
    if (this._rate && (finalSize > 0)) {
      remainingTime = Math.floor(((finalSize - currSize) / 1024) / this._rate);
      remainingTime = this._formatSeconds(remainingTime);
      remainingTime = this._replaceInsert(this._timeFormat, 1, remainingTime)
      remainingTime = this._replaceInsert(remainingTime, 2, this._remain);
    }
    
    //
    // [statusFormat:
    //  [progressFormat:
    //   [[progressFormatKBKB|
    //     progressFormatKBMB|
    //     progressFormatMBMB|
    //     progressFormatUnknownKB|
    //     progressFormatUnknownMB
    //    ][rateFormat]]
    //  ][timeFormat]
    // ]
    var status = this._statusFormat;
    status = this._replaceInsert(status, 1, progress);
    status = this._replaceInsert(status, 2, remainingTime);
    return status;
  },

  /**
   * Inserts a string into another string at the specified index, e.g. for
   * the format string var foo ="#1 #2 #3", |_replaceInsert(foo, 2, "test")|
   * returns "#1 test #3";
   * @param   format
   *          The format string
   * @param   index
   *          The Index to insert into
   * @param   value
   *          The value to insert
   * @returns The string with the value inserted. 
   */  
  _replaceInsert: function(format, index, value) {
    return format.replace(new RegExp("#" + index), value);
  },

  /**
   * Formats progress in the form of kilobytes transfered vs. total to 
   * transfer.
   * @param   currentKB
   *          The current amount of data transfered, in kilobytes.
   * @param   totalKB
   *          The total amount of data that must be transfered, in kilobytes.
   * @returns A string representation of the progress, formatted according to:
   * 
   *            KB           totalKB           returns
   *            x, < 1MB     y < 1MB           x of y KB
   *            x, < 1MB     y >= 1MB          x KB of y MB
   *            x, >= 1MB    y >= 1MB          x of y MB
   */
  _formatKBytes: function(currentKB, totalKB) {
    var progressHasMB = parseInt(currentKB / 1024) > 0;
    var totalHasMB = parseInt(totalKB / 1024) > 0;
    
    var format = "";
    if (!progressHasMB && !totalHasMB) {
      if (!totalKB) {
        format = this._progressFormatUnknownKB;
        format = this._replaceInsert(format, 1, currentKB);
      } else {
        format = this._progressFormatKBKB;
        format = this._replaceInsert(format, 1, currentKB);
        format = this._replaceInsert(format, 2, totalKB);
      }
    }
    else if (progressHasMB && totalHasMB) {
      format = this._progressFormatMBMB;
      format = this._replaceInsert(format, 1, (currentKB / 1024).toFixed(1));
      format = this._replaceInsert(format, 2, (totalKB / 1024).toFixed(1));
    }
    else if (totalHasMB && !progressHasMB) {
      format = this._progressFormatKBMB;
      format = this._replaceInsert(format, 1, currentKB);
      format = this._replaceInsert(format, 2, (totalKB / 1024).toFixed(1));
    }
    else if (progressHasMB && !totalHasMB) {
      format = this._progressFormatUnknownMB;
      format = this._replaceInsert(format, 1, (currentKB / 1024).toFixed(1));
    }
    return format;  
  },

  /**
   * Formats a time in seconds into something human readable.
   * @param   seconds
   *          The time to format
   * @returns A human readable string representing the date.
   */
  _formatSeconds: function(seconds) {
    // Determine number of hours/minutes/seconds
    var hours = (seconds - (seconds % 3600)) / 3600;
    seconds -= hours * 3600;
    var minutes = (seconds - (seconds % 60)) / 60;
    seconds -= minutes * 60;
    
    // Pad single digit values
    if (hours < 10)
      hours = "0" + hours;
    if (minutes < 10)
      minutes = "0" + minutes;
    if (seconds < 10)
      seconds = "0" + seconds;
    
    // Insert hours, minutes, and seconds into result string.
    var result = parseInt(hours) ? this._longTimeFormat : this._shortTimeFormat;
    result = this._replaceInsert(result, 1, hours);
    result = this._replaceInsert(result, 2, minutes);
    result = this._replaceInsert(result, 3, seconds);

    return result;
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
  _downloadName     : null,
  _downloadStatus   : null,
  _downloadProgress : null,
  _downloadThrobber : null,
  _pauseButton      : null,
  
  /**
   * Label cache to hold the 'Connecting' string
   */
  _label_downloadStatus : null,

  /** 
   * An instance of the status formatter object
   */
  _statusFormatter  : null,
  get statusFormatter() {
    if (!this._statusFormatter) 
      this._statusFormatter = new DownloadStatusFormatter();
    return this._statusFormatter;
  },
  
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
  
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);

    var um = 
        Components.classes["@mozilla.org/updates/update-manager;1"].
        getService(Components.interfaces.nsIUpdateManager);
    var activeUpdate = um.activeUpdate;
    if (activeUpdate)
      gUpdates.setUpdate(activeUpdate);
    
    if (!gUpdates.update) {
      LOG("UI:DownloadingPage", "onPageShow: no valid update to download?!");
      return;
    }
    
    try {
    // Say that this was a foreground download, not a background download, 
    // since the user cared enough to look in on this process.
    gUpdates.update.QueryInterface(Components.interfaces.nsIWritablePropertyBag);
    gUpdates.update.setProperty("foregroundDownload", "true");
  
    // Pause any active background download and restart it as a foreground
    // download.
    updates.pauseDownload();
    var state = updates.downloadUpdate(gUpdates.update, false);
    if (state == "failed") {
      // We've tried as hard as we could to download a valid update - 
      // we fell back from a partial patch to a complete patch and even
      // then we couldn't validate. Show a validation error with instructions
      // on how to manually update.
      this.showVerificationError();
    }
    else {
      // Add this UI as a listener for active downloads
      updates.addDownloadListener(this);
    }
    
    if (activeUpdate)
      this._setUIState(!updates.isDownloading);

    var link = document.getElementById("detailsLink");
    link.href = gUpdates.update.detailsURL;
    }
    catch(ex) {
      LOG("UI:DownloadingPage", "onPageShow: " + ex);
    }

    gUpdates.setButtons(null, true, null, true, null, true, "hideButton",
                        false, false, null, false, null, false);
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
   * Whether or not we are currently paused
   */
  _paused       : false,
  
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
      this._downloadName.value = gUpdates.strings.getFormattedString(
        "pausedName", [u.name]);
      this._pauseButton.label = gUpdates.strings.getString("pauseButtonResume");
      var p = u.selectedPatch.QueryInterface(Components.interfaces.nsIPropertyBag);
      var status = p.getProperty("status");
      if (status)
        this._setStatus(status);
    }
    else {
      if (!(this._downloadThrobber.hasAttribute("state") &&
           (this._downloadThrobber.getAttribute("state") == "loading")))
        this._downloadThrobber.setAttribute("state", "loading");
      if (this._downloadProgress.mode != "undetermined")
        this._downloadProgress.mode = "undetermined";
      this._downloadName.value = gUpdates.strings.getFormattedString(
        "downloadingPrefix", [u.name]);
      this._pauseButton.label = gUpdates.strings.getString("pauseButtonPause");
      this._setStatus(this._label_downloadStatus);
    }
  },

  /** 
   * When the user clicks the Pause/Resume button
   */
  onPause: function() {
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    if (this._paused)
      updates.downloadUpdate(gUpdates.update, false);
    else {
      var patch = gUpdates.update.selectedPatch;
      patch.QueryInterface(Components.interfaces.nsIWritablePropertyBag);
      patch.setProperty("status",
        gUpdates.strings.getFormattedString("pausedStatus", 
        [this.statusFormatter.progress]));
      updates.pauseDownload();
    }
    this._paused = !this._paused;
    
    // Update the UI
    this._setUIState(this._paused);
  },
  
  /** 
   * When the user closes the Wizard UI (including by clicking the Hide button)
   */
  onWizardCancel: function() {
    // Remove ourself as a download listener so that we don't continue to be 
    // fed progress and state notifications after the UI we're updating has 
    // gone away.
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    updates.removeDownloadListener(this);
    
    var um = 
        Components.classes["@mozilla.org/updates/update-manager;1"]
                  .getService(Components.interfaces.nsIUpdateManager);
    um.activeUpdate = gUpdates.update;
    
    // If the download was paused by the user, ask the user if they want to 
    // have the update resume in the background. 
    var downloadInBackground = true;
    if (this._paused) {
      var title = gUpdates.strings.getString("resumePausedAfterCloseTitle");
      var message = gUpdates.strings.getFormattedString(
        "resumePausedAfterCloseMessage", [gUpdates.brandName]);
      var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                        .getService(Components.interfaces.nsIPromptService);
      var flags = ps.STD_YES_NO_BUTTONS;
      // focus the software update wizard before prompting.
      // this will raise the software update wizard if it is minimized
      // making it more obvious what the prompt is for and will
      // solve the problem of windows "obscuring" the prompt.
      // see bug #350299 for more details
      window.focus();
      var rv = ps.confirmEx(window, title, message, flags, null, null, null, null, { });
      if (rv == 1) {
        downloadInBackground = false;
      }
    }
    if (downloadInBackground) {
      // Continue download in the background at full speed.
      LOG("UI:DownloadingPage", "onWizardCancel: continuing download in background at full speed");
      updates.downloadUpdate(gUpdates.update, false);
    }
  },
  
  /** 
   * When the data transfer begins
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   */
  onStartRequest: function(request, context) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("UI:DownloadingPage", "onStartRequest: " + request.URI.spec);
    
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
    request.QueryInterface(nsIIncrementalDownload);
    LOG("UI:DownloadingPage.onProgress", " " + request.URI.spec + ", " + progress + 
        "/" + maxProgress);

    var name = gUpdates.strings.getFormattedString("downloadingPrefix", [gUpdates.update.name]);
    var status = this.statusFormatter.formatStatus(progress, maxProgress);
    var progress = Math.round(100 * (progress/maxProgress));

    var p = gUpdates.update.selectedPatch;
    p.QueryInterface(Components.interfaces.nsIWritablePropertyBag);
    p.setProperty("progress", progress);
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
    this._downloadProgress.value = progress;
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
    request.QueryInterface(nsIIncrementalDownload);
    LOG("UI:DownloadingPage", "onStatus: " + request.URI.spec + " status = " + 
        status + ", text = " + statusText);
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
    request.QueryInterface(nsIIncrementalDownload);
    LOG("UI:DownloadingPage", "onStopRequest: " + request.URI.spec + 
        ", status = " + status);
    
    if (this._downloadThrobber.hasAttribute("state"))
      this._downloadThrobber.removeAttribute("state");
    if (this._downloadProgress.mode != "normal")
      this._downloadProgress.mode = "normal";

    var u = gUpdates.update;
    const NS_BINDING_ABORTED = 0x804b0002;
    switch (status) {
    case Components.results.NS_ERROR_UNEXPECTED:
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

        this._statusFormatter = null;
        return;
      }
      break;
    case NS_BINDING_ABORTED:
      LOG("UI:DownloadingPage", "onStopRequest: Pausing Download");
      // Return early, do not remove UI listener since the user may resume
      // downloading again.
      return;
    case Components.results.NS_OK:
      LOG("UI:DownloadingPage", "onStopRequest: Patch Verification Succeeded");
      gUpdates.wiz.canAdvance = true;
      gUpdates.wiz.advance();
      break;
    default:
      LOG("UI:DownloadingPage", "onStopRequest: Transfer failed");
      // Some kind of transfer error, die.
      gUpdates.wiz.currentPage = document.getElementById("errors");
      break;
    }

    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    updates.removeDownloadListener(this);
  },
  
  /**
   * See nsISupports.idl
   */
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIRequestObserver) &&
        !iid.equals(Components.interfaces.nsIProgressEventSink) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
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
    gUpdates.setButtons(null, true, null, true, null, false, "hideButton",
                        true, false, null, false, null, false);
    gUpdates.wiz.getButton("finish").focus();
    
    var errorReason = document.getElementById("errorReason");
    errorReason.value = gUpdates.update.statusText;
    var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                              .getService(Components.interfaces.nsIURLFormatter);
    var manualURL = formatter.formatURLPref(PREF_UPDATE_MANUAL_URL);
    var errorLinkLabel = document.getElementById("errorLinkLabel");
    errorLinkLabel.value = manualURL;
    errorLinkLabel.href = manualURL;
  },
  
  /**
   * Initialize, for the "Error Applying Patch" case.
   */
  onPageShowPatching: function() {
    gUpdates.wiz.getButton("back").disabled = true;
    gUpdates.wiz.getButton("cancel").disabled = true;
    gUpdates.wiz.getButton("next").focus();
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
    var updates =
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    updates.removeDownloadListener(gDownloadingPage);
  }
};

/**
 * The "Update has been downloaded" page. Shows information about what
 * was downloaded.
 */
var gFinishedPage = {
  /**
   * Called to initialize the Wizard Page.
   * @param   aDelayRestart
   *          true if the Restart Application button should be disabled for a
   *          short delay
   */
  onPageShow: function(aDelayRestart) {  
    gUpdates.setButtons(null, true, null, true, "restartButton", aDelayRestart,
                        "laterButton", false, false, null, false, null, false);
    if (aDelayRestart)
      setTimeout(this._enableRestartButton, 2000);
    else
      gUpdates.wiz.getButton("finish").focus();
  },
  
  /**
   * Called to initialize the Wizard Page. (Background Source Event)
   */
  onPageShowBackground: function() {
    var finishedBackground = document.getElementById("finishedBackground");
    finishedBackground.setAttribute("label", gUpdates.strings.getFormattedString(
      "updateReadyToInstallHeader", [gUpdates.update.name]));
    // XXXben - wizard should give us a way to set the page header.
    gUpdates.wiz._adjustWizardHeader();
    var updateFinishedName = document.getElementById("updateFinishedName");
    updateFinishedName.value = gUpdates.update.name;
    
    var link = document.getElementById("finishedBackgroundLink");
    link.href = gUpdates.update.detailsURL;
    
    this.onPageShow(true);

    if (getPref("getBoolPref", PREF_UPDATE_TEST_LOOP, false)) {
      window.restart = function () {
        gUpdates.wiz.getButton("finish").click();
      }
      setTimeout("restart();", UPDATE_TEST_LOOP_INTERVAL);
    }
  },
  
  /**
   * Re-enables and focuses the Restart Application button
   */
  _enableRestartButton: function() {
    gUpdates.wiz.canAdvance = true;
    var finishButton = gUpdates.wiz.getButton("finish");
    finishButton.disabled = false;
    finishButton.focus();
  },

  /**
   * Called when the wizard finishes, i.e. the "Restart Now" button is 
   * clicked. 
   */
  onWizardFinish: function() {
    // Do the restart
    LOG("UI:FinishedPage" , "onWizardFinish: Restarting Application...");
    
    // disable the "finish" (Restart) and "cancel" (Later) buttons
    // because the Software Update wizard is still up at the point,
    // and will remain up until we return and we close the
    // window with a |window.close()| in wizard.xml
    // (it was the firing the "wizardfinish" event that got us here.)
    // This prevents the user from switching back
    // to the Software Update dialog and clicking "Restart" or "Later"
    // when dealing with the "confirm close" prompts.
    // See bug #350299 for more details.
    gUpdates.wiz.getButton("finish").disabled = true;
    gUpdates.wiz.getButton("cancel").disabled = true;

    // This process is *extremely* broken. There should be some nice 
    // integrated system for determining whether or not windows are allowed
    // to close or not, and what happens when that happens. We need to 
    // jump through all these hoops (duplicated from globalOverlay.js) to
    // ensure that various window types (browser, downloads, etc) all 
    // allow the app to shut down. 
    // bsmedberg?     

    // Notify all windows that an application quit has been requested.
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    var cancelQuit = 
        Components.classes["@mozilla.org/supports-PRBool;1"].
        createInstance(Components.interfaces.nsISupportsPRBool);
    os.notifyObservers(cancelQuit, "quit-application-requested", null);

    // Something aborted the quit process. 
    if (cancelQuit.data)
      return;
    
    // Notify all windows that an application quit has been granted.
    os.notifyObservers(null, "quit-application-granted", null);

    // Enumerate all windows and call shutdown handlers
    var wm =
        Components.classes["@mozilla.org/appshell/window-mediator;1"].
        getService(Components.interfaces.nsIWindowMediator);
    var windows = wm.getEnumerator(null);
    while (windows.hasMoreElements()) {
      var win = windows.getNext();
      if (("tryToClose" in win) && !win.tryToClose())
        return;
    }
    
    var appStartup = 
        Components.classes["@mozilla.org/toolkit/app-startup;1"].
        getService(Components.interfaces.nsIAppStartup);
    appStartup.quit(appStartup.eAttemptQuit | appStartup.eRestart);
  },
  
  /**
   * Called when the wizard is canceled, i.e. when the "Later" button is
   * clicked.
   */
  onWizardCancel: function() {
    var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                       .getService(Components.interfaces.nsIPromptService);
    var message = gUpdates.strings.getFormattedString("restartLaterMsg",
      [gUpdates.brandName]);
    ps.alert(window, gUpdates.strings.getString("restartLaterTitle"), 
             message);

    var interval = getPref("getIntPref", PREF_UPDATE_NAGTIMER_RESTART, 1800);
    gUpdates.registerNagTimer("restart-nag-timer", interval, 
                              "showUpdateComplete");
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
    var ai = 
        Components.classes["@mozilla.org/xre/app-info;1"].
        getService(Components.interfaces.nsIXULAppInfo);
    
    var branding = document.getElementById("brandStrings");
    try {
      var url = branding.getFormattedString("whatsNewURL", [ai.version]);
      var whatsnewLink = document.getElementById("whatsnewLink");
      whatsnewLink.href = url;
      whatsnewLink.hidden = false;
    }
    catch (e) {
    }
    
    gUpdates.setButtons(null, true, null, true, null, false, "hideButton",
                        true, false, null, false, null, false);
    gUpdates.wiz.getButton("finish").focus();
  }
};

/**
 * Called as the application shuts down due to being quit from the File->Quit 
 * menu item.
 * XXXben this API is broken.
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
