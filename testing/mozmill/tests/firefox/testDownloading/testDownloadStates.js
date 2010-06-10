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
 * The Original Code is Mozmill Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Anthony Hughes <anthony.s.hughes@gmail.com>
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

var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['DownloadsAPI'];

var URL = "http://releases.mozilla.org/pub/mozilla.org/firefox/releases/3.6/source/firefox-3.6.source.tar.bz2";

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();
  module.dm = new DownloadsAPI.downloadManager();
  
  // Make sure Download Manager is clean before starting
  dm.cleanAll();
}

var teardownModule = function(module)
{
  dm.cleanAll();
  dm.close();
}

/* 
 * This tests all four download states:
 *   Pause, Resume, Cancel, and Retry
 */
var testDownloadStates = function()
{
  // Download a file
  DownloadsAPI.downloadFileOfUnknownType(controller, URL);
  
  // Wait for the Download Manager to open
  dm.waitForOpened(controller);
  
  // Get the download object
  var download = dm.getElement({type: "download", subtype: "id", value: "dl1"}); 
  controller.waitForElement(download);
  
  // Click the pause button and verify the download is paused
  var pauseButton = dm.getElement({type: "download_button", subtype: "pause", value: download});
  controller.click(pauseButton);
  dm.waitForDownloadState(download, DownloadsAPI.downloadState.paused);

  // Click the resume button and verify the download is active
  var resumeButton = dm.getElement({type: "download_button", subtype: "resume", value: download});
  controller.click(resumeButton);
  dm.waitForDownloadState(download, DownloadsAPI.downloadState.downloading);

  // Click the cancel button and verify the download is canceled
  var cancelButton = dm.getElement({type: "download_button", subtype: "cancel", value: download});
  controller.click(cancelButton);
  dm.waitForDownloadState(download, DownloadsAPI.downloadState.canceled);

  // Click the retry button and verify the download is active
  var retryButton = dm.getElement({type: "download_button", subtype: "retry", value: download});
  controller.click(retryButton);
  dm.waitForDownloadState(download, DownloadsAPI.downloadState.downloading);
}

