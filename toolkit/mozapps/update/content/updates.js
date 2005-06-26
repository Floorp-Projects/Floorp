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

const UPDATE_TEST_LOOP_INTERVAL     = 2000;

const URI_UPDATES_PROPERTIES  = "chrome://mozapps/locale/update/updates.properties";

const STATE_DOWNLOADING       = "downloading";
const STATE_PENDING           = "pending";
const STATE_APPLYING          = "applying";
const STATE_SUCCEEDED         = "succeeded";
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
  if (module in gLogEnabled) {
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

var gUpdates = {
  update      : null,
  strings     : null,
  brandName   : null,
  wiz         : null,
  sourceEvent : SRCEVT_FOREGROUND,
  
  /**
   * A hash of |pageid| attribute to page object. Can be used to dispatch
   * function calls to the appropriate page. 
   */
  _pages        : { },

  /**
   * Called when the user presses the "Finish" button on the wizard, dispatches
   * the function call to the selected page.
   */
  onWizardFinish: function() {
    var pageid = gUpdates.wiz.currentPage.pageid;
    if ("onWizardFinish" in this._pages[pageid])
      this._pages[pageid].onWizardFinish();
  },
  
  /**
   * Called when the user presses the "Cancel" button on the wizard, dispatches
   * the function call to the selected page.
   */
  onWizardCancel: function() {
    var pageid = gUpdates.wiz.currentPage.pageid;
    if ("onWizardCancel" in this._pages[pageid])
      this._pages[pageid].onWizardCancel();
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
    this.buttonLabel_back = gUpdates.wiz.getButton("back").label;
    this.buttonLabel_next = gUpdates.wiz.getButton("next").label;
    this.buttonLabel_finish = gUpdates.wiz.getButton("finish").label;
    this.buttonLabel_cancel = gUpdates.wiz.getButton("cancel").label;
    
    // Advance to the Start page. 
    gUpdates.wiz.currentPage = this.startPage;
  },

  /**
   *
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
   * U'Prompt Method:     Arg0:         Update State: Src Event:
   * showUpdateAvailable  nsIUpdate obj --            background
   * showUpdateComplete   nsIUpdate obj pending       background
   * checkForUpdates      null          --            foreground
   */
  get startPage() {
    if (window.arguments) {
      var arg0 = window.arguments[0];
      if (arg0 instanceof Components.interfaces.nsIUpdate) {
        // If the first argument is a nsIUpdate object, we are notifying the
        // user that the background checking found an update that requires
        // their permission to install, and it's ready for download.
        this.update = arg0;
        this.sourceEvent = SRCEVT_BACKGROUND;
        if (this.update.selectedPatch &&
            this.update.selectedPatch.state == STATE_PENDING)
          return document.getElementById("finishedBackground");
        return document.getElementById("updatesfound");
      }
    }
    else {
      var um = 
          Components.classes["@mozilla.org/updates/update-manager;1"].
          getService(Components.interfaces.nsIUpdateManager);
      if (um.activeUpdate) {
        this.update = um.activeUpdate;
        return document.getElementById("downloading");
      }
    }
    return document.getElementById("checking");
  },
  
  /**
   * Show the errors page.
   * @param   reason
   *          A text message explaining what the error was
   */
  advanceToErrorPage: function(reason) {
    var errorReason = document.getElementById("errorReason");
    errorReason.value = reason;
    var errorLink = document.getElementById("errorLink");
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch2);
    var manualURL = pref.getComplexValue(PREF_UPDATE_MANUAL_URL, 
      Components.interfaces.nsIPrefLocalizedString);
    errorLink.href = manualURL.data;
    var errorLinkLabel = document.getElementById("errorLinkLabel");
    errorLinkLabel.value = manualURL.data;
    
    var pageTitle = this.strings.getString("errorsPageHeader");
    
    var errorPage = document.getElementById("errors");
    errorPage.setAttribute("label", pageTitle);
    gUpdates.wiz.currentPage = document.getElementById("errors");
    gUpdates.wiz.setAttribute("label", pageTitle);
  },
  
  /**
   * Show a network error message.
   * @param   code
   *          An error code to look up in the properties file, either
   *          a Necko error code or a HTTP response code
   * @param   defaultCode
   *          The error code to fall back to if the specified code has no error
   *          text associated with it in the properties file.
   */
  advanceToErrorPageWithCode: function(code, defaultCode) {
    var sbs = 
        Components.classes["@mozilla.org/intl/stringbundle;1"].
        getService(Components.interfaces.nsIStringBundleService);
    var updateBundle = sbs.createBundle(URI_UPDATES_PROPERTIES);
    var reason = updateBundle.GetStringFromName("checker_error-" + defaultCode);
    try {
      reason = updateBundle.GetStringFromName("checker_error-" + code);
      LOG("General", "Transfer Error: " + reason + ", code: " + code);
    }
    catch (e) {
      // Use the default reason
      LOG("General", "Transfer Error: " + reason + ", code: " + defaultCode);
    }
    this.advanceToErrorPage(reason);
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
        this._prompter[methodName](this._update);
      }
    }
    tm.registerTimer(timerID, (new Callback(gUpdates.update, methodName)), 
                     timerInterval);
  },
}

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
    gUpdates.wiz.getButton("next").disabled = true;

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
      gUpdates.update = aus.selectUpdate(updates, updates.length);
      if (!gUpdates.update) {
        LOG("UI:CheckingPage", 
            "Could not select an appropriate update, either because there " + 
            "were none, or |selectUpdate| failed.");
        var checking = document.getElementById("checking");
        checking.setAttribute("next", "noupdatesfound");
      }
      gUpdates.wiz.advance();
    },

    /**
     * See nsIUpdateCheckListener
     */
    onError: function(request) {
      LOG("UI:CheckingPage", "UpdateCheckListener: error");
      
      try {
        var status = request.status;
      }
      catch (e) {
        var req = request.channel.QueryInterface(Components.interfaces.nsIRequest);
        status = req.status;
      }

      // If we can't find an error string specific to this status code, 
      // just use the 200 message from above, which means everything 
      // "looks" fine but there was probably an XML error or a bogus file.
      gUpdates.advanceToErrorPageWithCode(status, 200);
    },
    
    /**
     * See nsISupports.idl
     */
    QueryInterface: function(iid) {
      if (!aIID.equals(Components.interfaces.nsIUpdateCheckListener) &&
          !aIID.equals(Components.interfaces.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
    }
  }
};

var gNoUpdatesPage = {
  onPageShow: function() {
    gUpdates.wiz.getButton("back").disabled = true;
    gUpdates.wiz.getButton("cancel").disabled = true;
    gUpdates.wiz.getButton("finish").focus();
  }
};

var gUpdatesAvailablePage = {
  /**
   * An array of installed addons incompatible with this update.
   */
  _incompatibleItems: null,
  
  /**
   * Initialize.
   */
  onPageShow: function() {
    var updateName = gUpdates.strings.getFormattedString("updateName", 
      [gUpdates.brandName, gUpdates.update.version]);
    var updateNameElement = document.getElementById("updateName");
    updateNameElement.value = updateName;
    var displayType = gUpdates.strings.getString("updateType_" + gUpdates.update.type);
    var updateTypeElement = document.getElementById("updateType");
    updateTypeElement.setAttribute("type", gUpdates.update.type);
    var intro = gUpdates.strings.getFormattedString(
      "introType_" + gUpdates.update.type, [gUpdates.brandName]);
    while (updateTypeElement.hasChildNodes())
      updateTypeElement.removeChild(updateTypeElement.firstChild);
    updateTypeElement.appendChild(document.createTextNode(intro));
    
    var updateMoreInfoURL = document.getElementById("updateMoreInfoURL");
    updateMoreInfoURL.href = gUpdates.update.detailsURL;
    
    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    var items = em.getIncompatibleItemList("", gUpdates.update.version,
                                           nsIUpdateItem.TYPE_ADDON, { });
    if (items.length > 0) {
      // There are addons that are incompatible with this update, so show the 
      // warning message.
      var incompatibleWarning = document.getElementById("incompatibleWarning");
      incompatibleWarning.hidden = false;
      
      this._incompatibleItems = items;
    }
    
    // If we were invoked from a background update check, automatically show 
    // the additional details the user may need to make this decision since 
    // they did not consciously make the decision to check. 
    if (gUpdates.sourceEvent == SRCEVT_BACKGROUND)
      this.onShowMoreDetails();
    
    var downloadNow = document.getElementById("downloadNow");
    downloadNow.focus();
  },
  
  /**
   * User clicked the "More Details..." button
   */
  onShowMoreDetails: function() {
    var detailsDeck = document.getElementById("detailsDeck");
    detailsDeck.selectedIndex = 1;
  },
  
  /**
   * User said they wanted to install now, so advance to the License or 
   * Downloading page, whichever is appropriate.
   */
  onInstallNow: function() {
    var nextPageID = gUpdates.update.licenseURL ? "license" : "downloading";
    gUpdates.wiz.currentPage = document.getElementById(nextPageID);
  },
  
  /**
   * User said that they would install later. Register a timer to remind them
   * after a day or so.
   */
  onInstallLater: function() {
    var interval = getPref("getIntPref", PREF_UPDATE_NAGTIMER_DL, 86400000);
    gUpdates.registerNagTimer("download-nag-timer", interval, 
                              "showUpdateAvailable");

    // The user said "Later", so stop all update checks for this session
    // so that we don't bother them again.
    const nsIUpdateChecker = Components.interfaces.nsIUpdateChecker;
    var aus = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    aus.backgroundChecker.stopChecking(nsIUpdateChecker.CURRENT_SESSION);

    close();
  },
  
  /** 
   * Show a list of extensions made incompatible by this update.
   */
  showIncompatibleItems: function() {
    openDialog("chrome://mozapps/content/update/incompatible.xul", "", 
               "dialog,centerscreen,modal,resizable,titlebar", this._incompatibleItems);
  }
};

var gLicensePage = {
  _licenseContent: null,
  onPageShow: function() {
    this._licenseContent = document.getElementById("licenseContent");
    
    var nextButton = gUpdates.wiz.getButton("next");
    nextButton.disabled = true;
    nextButton.label = gUpdates.strings.getString("IAgreeLabel");
    gUpdates.wiz.getButton("back").disabled = true;
    gUpdates.wiz.getButton("next").focus();
    
    var cancelButton = gUpdates.wiz.getButton("cancel");
    cancelButton.label = gUpdates.strings.getString("IDoNotAgreeLabel");

    this._licenseContent.addEventListener("load", this.onLicenseLoad, false);
    this._licenseContent.url = gUpdates.update.licenseURL;
    
    gUpdates.wiz._wizardButtons.removeAttribute("type");
  },
  
  onLicenseLoad: function() {
    gUpdates.wiz.getButton("next").disabled = false;
  },
  
  onWizardCancel: function() {
    this._licenseContent.stopDownloading();
    
    // The user said "Do Not Agree", so stop all update checks for this session
    // so that we don't bother them again.
    const nsIUpdateChecker = Components.interfaces.nsIUpdateChecker;
    var aus = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    aus.backgroundChecker.stopChecking(nsIUpdateChecker.CURRENT_SESSION);
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
  this._statusFormatKBMB = us.getString("statusFormatKBMB");
  this._statusFormatKBKB = us.getString("statusFormatKBKB");
  this._statusFormatMBMB = us.getString("statusFormatMBMB");
  this._statusFormatUnknownMB = us.getString("statusFormatUnknownMB");
  this._statusFormatUnknownKB = us.getString("statusFormatUnknownKB");
  this._rateFormatKBSec = us.getString("rateFormatKBSec");
  this._rateFormatMBSec = us.getString("rateFormatMBSec");
  this._remain = us.getString("remain");
  this._unknownFilesize = us.getString("unknownFilesize");
  this._longTimeFormat = us.getString("longTimeFormat");
  this._shortTimeFormat = us.getString("shortTimeFormat");
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
      
      if (this._rate == 0)
        this._rateFormatted = "??.?";
      else {
        var format = isKB ? this._rateFormatKBSec : this._rateFormatMBSec;
        this._rateFormatted = this._replaceInsert(format, 1, this._rate);
      }
    }

    // 3) Determine the Time Remaining
    var remainingTime = this._unknownFileSize;
    if (this._rate && (finalSize > 0)) {
      remainingTime = Math.floor(((finalSize - currSize) / 1024) / this._rate);
      remainingTime = this._formatSeconds(remainingTime); 
    }
      
    var status = this._statusFormat;
    status = this._replaceInsert(status, 1, this.progress);
    status = this._replaceInsert(status, 2, this._rateFormatted);
    status = this._replaceInsert(status, 3, remainingTime);
    status = this._replaceInsert(status, 4, this._remain);
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
        format = this._statusFormatUnknownKB;
        format = this._replaceInsert(format, 1, currentKB);
      } else {
        format = this._statusFormatKBKB;
        format = this._replaceInsert(format, 1, currentKB);
        format = this._replaceInsert(format, 2, totalKB);
      }
    }
    else if (progressHasMB && totalHasMB) {
      format = this._statusFormatMBMB;
      format = this._replaceInsert(format, 1, (currentKB / 1024).toFixed(1));
      format = this._replaceInsert(format, 2, (totalKB / 1024).toFixed(1));
    }
    else if (totalHasMB && !progressHasMB) {
      format = this._statusFormatKBMB;
      format = this._replaceInsert(format, 1, currentKB);
      format = this._replaceInsert(format, 2, (totalKB / 1024).toFixed(1));
    }
    else if (progressHasMB && !totalHasMB) {
      format = this._statusFormatUnknownMB;
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
   * An instance of the status formatter object
   */
  _statusFormatter  : null,
  
  /** 
   *
   */
  onPageShow: function() {
    gUpdates.wiz._wizardButtons.removeAttribute("type");

    this._downloadName = document.getElementById("downloadName");
    this._downloadStatus = document.getElementById("downloadStatus");
    this._downloadProgress = document.getElementById("downloadProgress");
    this._downloadThrobber = document.getElementById("downloadThrobber");
    this._pauseButton = document.getElementById("pauseButton");
  
    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);

    var um = 
        Components.classes["@mozilla.org/updates/update-manager;1"].
        getService(Components.interfaces.nsIUpdateManager);
    var activeUpdate = um.activeUpdate;
    if (activeUpdate) {
      gUpdates.update = activeUpdate;
      this._togglePausedState(!updates.isDownloading);
    }
    
    if (!gUpdates.update) {
      LOG("UI:DownloadingPage.Progress", "no valid update to download?!");
      return;
    }
  
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
    
    gUpdates.wiz.getButton("back").disabled = true;
    var cancelButton = gUpdates.wiz.getButton("cancel");
    cancelButton.label = gUpdates.strings.getString("closeButtonLabel");
    cancelButton.focus();
    var nextButton = gUpdates.wiz.getButton("next");
    nextButton.disabled = true;
    nextButton.label = gUpdates.buttonLabel_next;
  },
  
  /** 
   *
   */
  _setStatus: function(status) {
    while (this._downloadStatus.hasChildNodes())
      this._downloadStatus.removeChild(this._downloadStatus.firstChild);
    this._downloadStatus.appendChild(document.createTextNode(status));
  },
  
  _paused       : false,
  _oldStatus    : null,
  _oldMode      : null,
  _oldProgress  : 0,
  
  /**
   * Adjust UI to suit a certain state of paused-ness
   * @param   paused
   *          Whether or not the download is paused
   */
  _togglePausedState: function(paused) {
    var u = gUpdates.update;
    var p = u.selectedPatch.QueryInterface(Components.interfaces.nsIPropertyBag);
    if (paused) {
      this._oldStatus = this._downloadStatus.textContent;
      this._oldMode = this._downloadProgress.mode;
      this._oldProgress = parseInt(this._downloadProgress.progress);
      this._downloadName.value = gUpdates.strings.getFormattedString(
        "pausedName", [u.name]);
      this._setStatus(p.getProperty("status"));
      this._downloadProgress.mode = "normal";
      
      this._pauseButton.label = gUpdates.strings.getString("pauseButtonResume");
    }
    else {
      this._downloadName.value = gUpdates.strings.getFormattedString(
        "downloadingPrefix", [u.name]);
      this._setStatus(this._oldStatus || p.getProperty("status"));
      this._downloadProgress.value = 
        this._oldProgress || parseInt(p.getProperty("progress"));
      this._downloadProgress.mode = this._oldMode || "normal";
      this._pauseButton.label = gUpdates.strings.getString("pauseButtonPause");
    }
  },

  /** 
   *
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
          [this._statusFormatter.progress]));
      updates.pauseDownload();
    }
    this._paused = !this._paused;
    
    // Update the UI
    this._togglePausedState(this._paused);
  },
  
  /** 
   *
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
      var rv = ps.confirmEx(window, title, message, flags, null, null, null, null, { });
      if (rv == 1) {
        downloadInBackground = false;
      }
    }
    if (downloadInBackground) {
      // Cancel the download and start it again in the background.
      LOG("UI:DownloadingPage", "onWizardCancel: resuming download in background");
      updates.pauseDownload();
      updates.downloadUpdate(gUpdates.update, true);
    }
  },
  
  /** 
   *
   */
  onStartRequest: function(request, context) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("UI:DownloadingPage", "onStartRequest: " + request.URI.spec);

    this._statusFormatter = new DownloadStatusFormatter();
    
    this._downloadThrobber.setAttribute("state", "loading");
  },
  
  /** 
   *
   */
  onProgress: function(request, context, progress, maxProgress) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("UI:DownloadingPage.onProgress", request.URI.spec + ", " + progress + 
        "/" + maxProgress);

    var p = gUpdates.update.selectedPatch;
    p.QueryInterface(Components.interfaces.nsIWritablePropertyBag);
    p.setProperty("progress", Math.round(100 * (progress/maxProgress)));
    p.setProperty("status", 
      this._statusFormatter.formatStatus(progress, maxProgress));

    this._downloadProgress.mode = "normal";
    this._downloadProgress.value = parseInt(p.getProperty("progress"));
    this._pauseButton.disabled = false;
    var name = gUpdates.strings.getFormattedString("downloadingPrefix", [gUpdates.update.name]);
    this._downloadName.value = name;
    this._setStatus(p.getProperty("status"));
  },
  
  /** 
   *
   */
  onStatus: function(request, context, status, statusText) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("UI:DownloadingPage", "onStatus: " + request.URI.spec + " status = " + 
        status + ", text = " + statusText);
  },
  
  /** 
   *
   */
  onStopRequest: function(request, context, status) {
    request.QueryInterface(nsIIncrementalDownload);
    LOG("UI:DownloadingPage", "onStopRequest: " + request.URI.spec + 
        ", status = " + status);
    
    this._downloadThrobber.removeAttribute("state");

    const NS_BINDING_ABORTED = 0x804b0002;
    switch (status) {
    case Components.results.NS_ERROR_UNEXPECTED:  
      LOG("UI:DownloadingPage", "STATE = " + gUpdates.update.selectedPatch.state);
      if (gUpdates.update.selectedPatch.state == STATE_FAILED)
        this.showVerificationError();
      else {
        LOG("UI:DownloadingPage", "TYPE = " + gUpdates.update.selectedPatch.type);
        // Verification failed for a partial patch, complete patch is now
        // downloading so return early and do NOT remove the download listener!
        
        // Reset the progress meter to "undertermined" mode so that we don't 
        // show old progress for the new download of the "complete" patch.
        this._downloadProgress.mode = "undetermined";
        this._pauseButton.disabled = true;
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
      gUpdates.wiz.advance();
      break;
    default:
      LOG("UI:DownloadingPage", "onStopRequest: Transfer failed");
      // Some kind of transfer error, die.
      gUpdates.advanceToErrorPageWithCode(status, 2152398849);
      break;
    }

    var updates = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    updates.removeDownloadListener(this);
  },
  
  /** 
   * Advance the wizard to the "Verification Error" page
   */
  showVerificationError: function() {
    var verificationError = gUpdates.strings.getFormattedString(
      "verificationError", [gUpdates.brandName]);
    var downloadingPage = document.getElementById("downloading");
    gUpdates.advanceToErrorPage(verificationError);
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

var gErrorsPage = {
  onPageShow: function() {
    gUpdates.wiz.getButton("back").disabled = true;
    gUpdates.wiz.getButton("cancel").disabled = true;
    gUpdates.wiz.getButton("finish").focus();
  }
};

var gFinishedPage = {
  /**
   * Called to initialize the Wizard Page.
   */
  onPageShow: function() {
    v.getButton("back").disabled = true;
    var finishButton = gUpdates.wiz.getButton("finish");
    finishButton.label = gUpdates.strings.getString("restartButton");
    finishButton.focus();
    var cancelButton = gUpdates.wiz.getButton("cancel");
    cancelButton.label = gUpdates.strings.getString("laterButton");
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
    
    this.onPageShow();

    if (getPref("getBoolPref", PREF_UPDATE_TEST_LOOP, false)) {
      window.restart = function () {
        gUpdates.wiz.getButton("finish").click();
      }
      setTimeout("restart();", UPDATE_TEST_LOOP_INTERVAL);
    }
  },
  
  /**
   * Called when the wizard finishes, i.e. the "Restart Now" button is 
   * clicked. 
   */
  onWizardFinish: function() {
    // Do the restart
    LOG("UI:FinishedPage" , "onWizardFinish: Restarting Application...");
    
    // This process is *extremely* retarded. There should be some nice 
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
    var message = gUpdates.strings.getFormattedString("restartLaterMessage",
      [gUpdates.brandName]);
    ps.alert(window, gUpdates.strings.getString("restartLaterTitle"), 
             message);

    var interval = getPref("getIntPref", PREF_UPDATE_NAGTIMER_RESTART, 
                           18000000);
    gUpdates.registerNagTimer("restart-nag-timer", interval, 
                              "showUpdateComplete");
  },
};

/**
 * Called as the application shuts down due to being quit from the File->Quit 
 * menu item.
 * XXXben this API is retarded.
 */
function tryToClose() {
  var cp = gUpdates.wiz.currentPage;
  if (cp.pageid != "finished" && cp.pageid != "finishedBackground")
    gUpdates.onWizardCancel();
  return true;
}

