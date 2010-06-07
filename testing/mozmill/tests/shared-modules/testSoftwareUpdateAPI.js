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
 *
 * @version 1.0.0
 */

const MODULE_NAME = 'SoftwareUpdateAPI';

const RELATIVE_ROOT = '.';
const MODULE_REQUIRES = ['PrefsAPI'];

const gTimeout                = 5000;
const gTimeoutUpdateCheck     = 10000;
const gTimeoutUpdateDownload  = 360000;

/**
 * Constructor for software update class
 */
function softwareUpdate()
{
  this._controller = null;
  this._wizard = null;

  this._aus = Cc["@mozilla.org/updates/update-service;1"]
                 .getService(Ci.nsIApplicationUpdateService);
  this._ums = Cc["@mozilla.org/updates/update-manager;1"]
                 .getService(Ci.nsIUpdateManager);

  // Get all available buttons for later clicks
  // http://mxr.mozilla.org/mozilla-central/source/toolkit/content/widgets/wizard.xml#467
  if (mozmill.isMac) {
    var template = '/id("updates")/anon({"anonid":"Buttons"})/anon({"flex":"1"})' +
                   '/{"class":"wizard-buttons-btm"}/';
    this._buttons = {
                      back: template + '{"dlgtype":"back"}',
                      next: template + '{"dlgtype":"next"}',
                      cancel: template + '{"dlgtype":"cancel"}',
                      finish: template + '{"dlgtype":"finish"}',
                      extra1: template + '{"dlgtype":"extra1"}',
                      extra2: template + '{"dlgtype":"extra2"}'
                    };
  } else if (mozmill.isLinux || mozmill.isWindows) {
    var template = '/id("updates")/anon({"anonid":"Buttons"})/anon({"flex":"1"})' +
                   '/{"class":"wizard-buttons-box-2"}/';
    this._buttons = {
                      back: template + '{"dlgtype":"back"}',
                      next: template + 'anon({"anonid":"WizardButtonDeck"})/[1]' +
                                       '/{"dlgtype":"next"}',
                      cancel: template + '{"dlgtype":"cancel"}',
                      finish: template + 'anon({"anonid":"WizardButtonDeck"})/[0]' +
                                         '/{"dlgtype":"finish"}',
                      extra1: template + '{"dlgtype":"extra1"}',
                      extra2: template + '{"dlgtype":"extra2"}'
                    };
  }
}

/**
 * Class for software updates
 */
softwareUpdate.prototype = {

  /**
   * Returns the active update
   */
  get activeUpdate() {
    return this._ums.activeUpdate;
  },

  /**
   * Check if the user has permissions to run the software update
   */
  get allowed() {
    return this._aus.canUpdate;
  },

  /**
   * Returns the current step of the software update dialog wizard
   */
  get currentStep() {
    return this._wizard.getAttribute('currentpageid');
  },

  /**
   * Returns if the offered update is a complete update
   */
  get isCompleteUpdate() {
    // XXX: Bug 514040: _ums.isCompleteUpdate doesn't work at the moment
    if (this.activeUpdate.patchCount > 1) {
      var patch1 = this.activeUpdate.getPatchAt(0);
      var patch2 = this.activeUpdate.getPatchAt(1);

      return (patch1.URL == patch2.URL);
    } else {
      return (this.activeUpdate.getPatchAt(0).type == "complete");
    }
  },

  /**
   * Returns the update type (minor or major)
   */
  get updateType() {
    updateType = new elementslib.ID(this._controller.window.document, "updateType");
    return updateType.getNode().getAttribute("severity");
  },

  /**
   * Asserts the given step of the update dialog wizard
   * Available steps: dummy, checking, noupdatesfound, incompatibleCheck,
   *                  updatesfound, license, incompatibleList, downloading,
   *                  errors, errorpatching, finished, finishedBackground,
   *                  installed
   */
  assertUpdateStep : function softwareUpdate_assertUpdateStep(step) {
    this._controller.waitForEval("subject.currentStep == '" + step + "'",
                                 gTimeout, 100, this);
  },

  /**
   * Close the software update dialog
   */
  closeDialog: function softwareUpdate_closeDialog() {
    if (this._controller) {
      this._controller.keypress(null, "VK_ESCAPE", {});
      this._controller.sleep(500);
      this._controller = null;
    }
  },

  /**
   * Download the update of the given channel and type
   * @param {string} updateType
   *        Update type of the update (minor or major)
   * @param {string} channel
   *        Update channel to use
   */
  download : function softwareUpdate_download(updateType, channel, timeout) {
    timeout = timeout ? timeout : gTimeoutUpdateDownload;

    if (this.currentStep == "updatesfound") {
      // Check if the correct channel has been set
      var prefs = collector.getModule('PrefsAPI').preferences;

      this._controller.assertJS("subject.currentChannel == subject.expectedChannel",
                                {currentChannel: channel, expectedChannel: prefs.getPref('app.update.channel','')});

      // Only allow updates of the given type
      this._controller.assertJS("subject.expectedUpdateType == subject.lastUpdateType",
                                {expectedUpdateType: updateType, lastUpdateType: this.updateType});
    }

    // Click the next button
    var next = new elementslib.Lookup(this._controller.window.document,
                                      this._buttons.next);
    this._controller.click(next);

    // Wait until update has been downloaded
    var progress = this._controller.window.document.getElementById("downloadProgress");
    this._controller.waitForEval("subject.value == 100", timeout, 100, progress);
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
   * Open software update dialog and get window controller
   * @param {MozMillController} browserController
   *        Mozmill controller of the browser window
   */
  openDialog: function softwareUpdate_openDialog(browserController) {
    // Allow only one instance of the controller
    if (this._controller)
      return;

    var updateMenu = new elementslib.Elem(browserController.menus.helpMenu.checkForUpdates);
    browserController.click(updateMenu);

    this.waitForDialogOpen(browserController);
  },

  /**
   * Wait that check for updates has been finished
   * @param {number} timeout
   */
  waitForCheckFinished : function softwareUpdate_waitForCheckFinished(timeout) {
    timeout = timeout ? timeout : gTimeoutUpdateCheck;

    this._controller.waitForEval("subject.currentStep != 'checking'", timeout, 100, this);
  },

  /**
   * Wait for the software update dialog
   * @param {MozMillController} browserController
   *        Mozmill controller of the browser window
   */
  waitForDialogOpen : function softwareUpdate_waitForDialogOpen(browserController) {
    browserController.sleep(500);

    var window = mozmill.wm.getMostRecentWindow('Update:Wizard');
    this._controller = new mozmill.controller.MozMillController(window);
    this._wizard = this._controller.window.document.getElementById('updates');

    // Wait until the dummy wizard page isn't visible anymore
    this._controller.waitForEval("subject.currentStep != 'dummy'", gTimeout, 100, this);
    this._controller.window.focus();
  }
}
