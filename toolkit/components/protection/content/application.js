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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


/**
 * A simple helper class that enables us to get or create the
 * directory in which our app will store stuff.
 */
function PROT_ApplicationDirectory() {
  this.debugZone = "appdir";
  this.appDir_ = G_File.getProfileFile();
  this.appDir_.append(PROT_GlobalStore.getAppDirectoryName());
  G_Debug(this, "Application directory is " + this.appDir_.path);
}

/**
 * @returns Boolean indicating if the directory exists
 */
PROT_ApplicationDirectory.prototype.exists = function() {
  return this.appDir_.exists() && this.appDir_.isDirectory();
}

/**
 * Creates the directory
 */
PROT_ApplicationDirectory.prototype.create = function() {
  G_Debug(this, "Creating app directory: " + this.appDir_.path);
  try {
    this.appDir_.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
  } catch(e) {
    G_Error(this, this.appDir_.path + " couldn't be created.");
  }
}

/**
 * @returns The nsIFile interface of the directory
 */
PROT_ApplicationDirectory.prototype.getAppDirFileInterface = function() {
  return this.appDir_;
}
