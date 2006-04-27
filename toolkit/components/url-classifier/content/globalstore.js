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


// A class that encapsulates globals such as the names of things.  We
// centralize everything here mainly so as to ease their modification,
// but also in case we want to version.
//
// This class does _not_ embody semantics, defaults, or the like. If we
// need something that does, we'll add our own preference registry.
//
// TODO: The code needs to fail more gracefully if these values aren't set
//       E.g., createInstance should fail for listmanager without these.

/**
 * A clearinghouse for globals. All interfaces are read-only.
 */
function PROT_GlobalStore() {
}

/**
 * Read a pref value
 */
PROT_GlobalStore.getPref_ = function(prefname) {
  var pref = new G_Preferences();
  return pref.getPref(prefname);
}

/**
 * @returns The name of the directory in which we should store data.
 */
PROT_GlobalStore.getAppDirectoryName = function() {
  return PROT_GlobalStore.getPref_("safebrowsing.datadirectory");
}

/**
 * @returns String giving url to use for updates, e.g.,
 *                 http://www.google.com/safebrowsing/update?
 */
PROT_GlobalStore.getUpdateserverURL = function() {
  // TODO: handle multiple providers
  return PROT_GlobalStore.getPref_("safebrowsing.provider.0.updateurl");
}

/**
 * @returns String giving url to use for re-keying, e.g.,
 *                 https://www.google.com/safebrowsing/getkey?
 */
PROT_GlobalStore.getGetKeyURL = function() {
  return PROT_GlobalStore.getPref_("safebrowsing.provider.0.keyurl");
}

/**
 * @returns String giving filename to use to store keys
 */
PROT_GlobalStore.getKeyFilename = function() {
  return "kf.txt";
}

/**
 * For running unittests.
 * TODO: make this automatic based on build rules
 */
PROT_GlobalStore.isTesting = function() {
  return true;
}
