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
 * The Original Code is XPI Dialog Service.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// -----------------------------------------------------------------------
// Directory Provider for special browser folders and files
// -----------------------------------------------------------------------

const NS_APP_CACHE_PARENT_DIR = "cachePDir";
const XRE_UPDATE_ROOT_DIR     = "UpdRootD";
const ENVVAR_UPDATE_DIR       = "UPDATES_DIRECTORY";

function DirectoryProvider() {}

DirectoryProvider.prototype = {
  classID: Components.ID("{ef0f7a87-c1ee-45a8-8d67-26f586e46a4b}"),
  
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider]),

  getFile: function(prop, persistent) {
    if (prop == NS_APP_CACHE_PARENT_DIR) {
      let dirsvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
      let profile = dirsvc.get("ProfD", Ci.nsIFile);

      let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
      let device = sysInfo.get("device");
      switch (device) {
#ifdef MOZ_PLATFORM_MAEMO
        case "Nokia N900":
          return profile;
        
        case "Nokia N8xx":
          let folder = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
          folder.initWithPath("/media/mmc2/.mozilla/fennec");
          return folder;
#endif
        default:
          return profile;
      }
    } else if (prop == XRE_UPDATE_ROOT_DIR) {
      let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
      if (env.exists(ENVVAR_UPDATE_DIR)) {
        let path = env.get(ENVVAR_UPDATE_DIR);
        if (path) {
          let localFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
          localFile.initWithPath(path);
          return localFile;
        }
      }
      let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
      return dm.defaultDownloadsDirectory;
    }
    
    // We are retuning null to show failure instead for throwing an error. The
    // interface is called quite a bit and throwing an error is noisy. Returning
    // null works with the way the interface is called [see bug 529077]
    return null;
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DirectoryProvider]);

