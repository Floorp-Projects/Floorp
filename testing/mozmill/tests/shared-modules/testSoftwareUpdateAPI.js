/* * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
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
 * **** END LICENSE BLOCK ***** */

/**
 * @fileoverview
 * The SoftwareUpdateAPI adds support for an easy access to the update process.
 */

const MODULE_NAME = 'SoftwareUpdateAPI';

const RELATIVE_ROOT = '.';
const MODULE_REQUIRES = ['PrefsAPI', 'UtilsAPI'];

const gTimeout                = 5000;
const gTimeoutUpdateCheck     = 10000;
const gTimeoutUpdateDownload  = 360000;

// Helper lookup constants for elements of the software update dialog
const WIZARD = '/id("updates")';
const WIZARD_BUTTONS = WIZARD + '/anon({"anonid":"Buttons"})';
const WIZARD_DECK = WIZARD  + '/anon({"anonid":"Deck"})';

const WIZARD_PAGES = {
  dummy: 'dummy',
  checking: 'checking',
  pluginUpdatesFound: 'pluginupdatesfound',
  noUpdatesFound: 'noupdatesfound',
  manualUpdate: 'manualUpdate',
  incompatibleCheck: 'incompatibleCheck',
  updatesFoundBasic: 'updatesfoundbasic',
  updatesFoundBillboard: 'updatesfoundbillboard',
  license: 'license',
  incompatibleList: 'incompatibleList',
  downloading: 'downloading',
  errors: 'errors',
  errorPatching: 'errorpatching',
  finished: 'finished',
  finishedBackground: 'finishedBackground',
  installed: 'installed'
}

// On Mac there is another DOM structure used as on Windows and Linux
if (mozmill.isMac) {
  var WIZARD_BUTTONS_BOX = WIZARD_BUTTONS +
                             '/anon({"flex":"1"})/{"class":"wizard-buttons-btm"}/';
  var WIZARD_BUTTON = {
          back: '{"dlgtype":"back"}',
          next: '{"dlgtype":"next"}',
          cancel: '{"dlgtype":"cancel"}',
          finish: '{"dlgtype":"finish"}',
          extra1: '{"dlgtype":"extra1"}',
          extra2: '{"dlgtype":"extra2"}'
        }
} else {
  var WIZARD_BUTTONS_BOX = WIZARD_BUTTONS +
                       '/anon({"flex":"1"})/{"class":"wizard-buttons-box-2"}/';
  var WIZARD_BUTTON = {
    back: '{"dlgtype":"back"}',
    next: 'anon({"anonid":"WizardButtonDeck"})/[1]/{"dlgtype":"next"}',
    cancel: '{"dlgtype":"cancel"}',
    finish: 'anon({"anonid":"WizardButtonDeck"})/[0]/{"dlgtype":"finish"}',
    extra1: '{"dlgtype":"extra1"}',
    extra2: '{"dlgtype":"extra2"}'
  }
}

/**
 * Constructor for software update class
 */
function softwareUpdate()
{
  this._controller = null;
  this._wizard = null;

  this._prefsAPI = collector.getModule('PrefsAPI');
  this._utilsAPI = collector.getModule('UtilsAPI');

  this._aus = Cc["@mozilla.org/updates/update-service;1"].
              getService(Ci.nsIApplicationUpdateService);
  // nsIApplicationUpdateService2 is required for Firefox 3.6 but doesn't exist
  // in Firefox 4.0. This will QI to nsIApplicationUpdateService2 only when it
  // is available.
  if ("nsIApplicationUpdateService2" in Ci) {
    this._aus.QueryInterface(Ci.nsIApplicationUpdateService2);
  }
  this._ums = Cc["@mozilla.org/updates/update-manager;1"].
              getService(Ci.nsIUpdateManager);
}

/**
 * Class for software updates
 */
softwareUpdate.prototype = {
  /**
   * Returns the active update
   *
   * @returns The currently selected update
   * @type nsIUpdate
   */
  get activeUpdate() {
    return this._ums.activeUpdate;
  },

  /**
   * Check if the user has permissions to run the software update
   *
   * @returns Status if the user has the permissions.
   * @type {boolean}
   */
  get allowed() {
    return this._aus.canCheckForUpdates && this._aus.canApplyUpdates;
  },

  /**
   * Get the controller of the associated engine manager dialog
   *
   * @returns Controller of the browser window
   * @type MozMillController
   */
  get controller() {
    return this._controller;
  },

  /**
   * Returns the current step of the software update dialog wizard
   */
  get currentPage() {
    return this._wizard.getNode().getAttribute('currentpageid');
  },

  /**
   * Returns true if the offered update is a complete update
   */
  get isCompleteUpdate() {
    // Throw when isCompleteUpdate is called without an update. This should
    // never happen except if the test is incorrectly written.
    if (!this.activeUpdate)
      throw new Error(arguments.callee.name + ": isCompleteUpdate called " +
                      "when activeUpdate is null!");

    var patchCount = this.activeUpdate.patchCount;
    // Test that the update snippet created by releng has less than 3 patches
    controller.assertJS("subject.patchCount < 3",
                        {patchCount: patchCount < 3});
    // Test that the update snippet created by releng has more than 0 patches
    controller.assertJS("subject.patchCount > 0",
                        {patchCount: patchCount > 0});

// After bug 514040 is fixed remove this line and uncomment out the following
// code
//    if (this.activeUpdate.patchCount == 2) {
//      var patch0URL = this.activeUpdate.getPatchAt(0).URL;
//      var patch1URL = this.activeUpdate.getPatchAt(1).URL;
      // Test that the update snippet created by releng doesn't have the same
      // url for both patches (bug 514040).
//      controller.assertJS("subject.patch0URL != subject.patch1URL",
//                          {patch0URL: patch0URL, patch1URL: patch1URL});
//    }

    return (this.activeUpdate.selectedPatch.type  == "complete");
  },

  /**
   * Returns the update type (minor or major)
   *
   * @returns The update type
   */
  get updateType() {
    return this.activeUpdate.type;
  },

  /**
   * Check if updates have been found
   */
  get updatesFound() {
    return this.currentPage.indexOf("updatesfound") == 0;
  },

  /**
   * Checks if an update has been applied correctly
   *
   * @param {object} updateData
   *        All the data collected during the update process
   */
  assertUpdateApplied : function softwareUpdate_assertUpdateApplied(updateData) {
    // The upgraded version should be identical with the version given by
    // the update and we shouldn't have run a downgrade
    var vc = Cc["@mozilla.org/xpcom/version-comparator;1"].
             getService(Ci.nsIVersionComparator);
    var check = vc.compare(updateData.postVersion, updateData.preVersion);
  
    controller.assertJS("subject.newVersionGreater == true",
                        {newVersionGreater: check >= 0});
  
    // If we have the same version number we should check the build id instead
    if (check == 0) {
      controller.assertJS("subject.postBuildId == subject.updateBuildId",
                          {postBuildId: updateData.postBuildId, updateBuildId: updateData.updateBuildId});
    }
  
    // An upgrade should not change the builds locale
    controller.assertJS("subject.postLocale == subject.preLocale",
                        {postLocale: updateData.postLocale, preLocale: updateData.preLocale});
  },

  /**
   * Close the software update dialog
   */
  closeDialog: function softwareUpdate_closeDialog() {
    if (this._controller) {
      this._controller.keypress(null, "VK_ESCAPE", {});
      this._controller.sleep(500);
      this._controller = null;
      this._wizard = null;
    }
  },

  /**
   * Download the update of the given channel and type
   * @param {string} channel
   *        Update channel to use
   * @param {boolean} waitForFinish
   *        Sets if the function should wait until the download has been finished
   * @param {number} timeout
   *        Timeout the download has to stop
   */
  download : function softwareUpdate_download(channel, waitForFinish, timeout) {
    waitForFinish = waitForFinish ? waitForFinish : true;

    // Check that the correct channel has been set
    var prefChannel = this._prefsAPI.preferences.getPref('app.update.channel', '');
    this._controller.assertJS("subject.currentChannel == subject.expectedChannel",
                              {currentChannel: channel, expectedChannel: prefChannel});

    // Click the next button
    var next = this.getElement({type: "button", subtype: "next"});
    this._controller.click(next);

    // Wait for the download page
    this.waitForWizardPage(WIZARD_PAGES.downloading);

    if (waitForFinish)
      this.waitforDownloadFinished(timeout);
  },

  /**
   * Update the update.status file and set the status to 'failed:6'
   */
  forceFallback : function softwareUpdate_forceFallback() {
    var dirService = Cc["@mozilla.org/file/directory_service;1"]
                        .getService(Ci.nsIProperties);

    var updateDir;
    var updateStatus;

    // Check the global update folder first
    try {
      updateDir = dirService.get("UpdRootD", Ci.nsIFile);
      updateDir.append("updates");
      updateDir.append("0");

      updateStatus = updateDir.clone();
      updateStatus.append("update.status");
    } catch (ex) {
    }

    if (updateStatus == undefined || !updateStatus.exists()) {
      updateDir = dirService.get("XCurProcD", Ci.nsIFile);
      updateDir.append("updates");
      updateDir.append("0");

      updateStatus = updateDir.clone();
      updateStatus.append("update.status");
    }

    var foStream = Cc["@mozilla.org/network/file-output-stream;1"]
                      .createInstance(Ci.nsIFileOutputStream);
    var status = "failed: 6\n";
    foStream.init(updateStatus, 0x02 | 0x08 | 0x20, -1, 0);
    foStream.write(status, status.length);
    foStream.close();
  },

  /**
   * Gets all the needed external DTD urls as an array
   *
   * @returns Array of external DTD urls
   * @type [string]
   */
  getDtds : function softwareUpdate_getDtds() {
    var dtds = ["chrome://mozapps/locale/update/history.dtd",
                "chrome://mozapps/locale/update/updates.dtd"]
    return dtds;
  },

  /**
   * Retrieve an UI element based on the given spec
   *
   * @param {object} spec
   *        Information of the UI element which should be retrieved
   *        type: General type information
   *        subtype: Specific element or property
   *        value: Value of the element or property
   * @returns Element which has been created  
   * @type {ElemBase}
   */
  getElement : function softwareUpdate_getElement(spec) {
    var elem = null;

    switch(spec.type) {
      /**
       * subtype: subtype to match
       * value: value to match
       */
      case "button":
        elem = new elementslib.Lookup(this._controller.window.document,
                                      WIZARD_BUTTONS_BOX + WIZARD_BUTTON[spec.subtype]);
        break;
      case "wizard":
        elem = new elementslib.Lookup(this._controller.window.document, WIZARD);
        break;
      case "wizard_page":
        elem = new elementslib.Lookup(this._controller.window.document, WIZARD_DECK +
                                      '/id(' + spec.subtype + ')');
        break;
      case "download_progress":
        elem = new elementslib.ID(this._controller.window.document, "downloadProgress");
        break;
      case "menu_update":
        elem = new elementslib.Elem(spec.value.menus.helpMenu.checkForUpdates);
        break;
      default:
        throw new Error(arguments.callee.name + ": Unknown element type - " + spec.type);
    }

    return elem;
  },

  /**
   * Open software update dialog
   *
   * @param {MozMillController} browserController
   *        Mozmill controller of the browser window
   */
  openDialog: function softwareUpdate_openDialog(browserController) {
    var updateMenu = this.getElement({type: "menu_update",
                                      value: browserController});
    
    browserController.click(updateMenu);

    this.waitForDialogOpen(browserController);
  },

  /**
   * Wait that check for updates has been finished
   * @param {number} timeout
   */
  waitForCheckFinished : function softwareUpdate_waitForCheckFinished(timeout) {
    timeout = timeout ? timeout : gTimeoutUpdateCheck;

    this._controller.waitForEval("subject.wizard.currentPage != subject.checking", timeout, 100,
                                 {wizard: this, checking: WIZARD_PAGES.checking});
  },

  /**
   * Wait for the software update dialog
   *
   * @param {MozMillController} browserController
   *        Mozmill controller of the browser window
   */
  waitForDialogOpen : function softwareUpdate_waitForDialogOpen(browserController) {
    this._controller = this._utilsAPI.handleWindow("type", "Update:Wizard",
                                                   null, true);
    this._wizard = this.getElement({type: "wizard"});

    // Wait until the dummy wizard page isn't visible anymore
    this._controller.waitForEval("subject.wizard.currentPage != subject.dummy", gTimeout, 100,
                                 {wizard: this, dummy: WIZARD_PAGES.dummy});
    this._controller.window.focus();
  },

  /**
   * Wait until the download has been finished
   *
   * @param {number} timeout
   *        Timeout the download has to stop
   */
  waitforDownloadFinished: function softwareUpdate_waitForDownloadFinished(timeout) {
    timeout = timeout ? timeout : gTimeoutUpdateDownload;

    // Wait until the update has been downloaded
    var progress =  this.getElement({type: "download_progress"});
    this._controller.waitForEval("subject.progress.value == 100", timeout, 100,
                                 {progress: progress.getNode()});

    this.waitForWizardPage(WIZARD_PAGES.finished);
  },

  /**
   * Waits for the given page of the update dialog wizard
   */
  waitForWizardPage : function softwareUpdate_waitForWizardPage(step) {
    this._controller.waitForEval("subject.currentPage == '" + step + "'",
                                 gTimeout, 100, this);
  }
}
