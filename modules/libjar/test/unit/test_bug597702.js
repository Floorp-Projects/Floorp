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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 *  Michael Wu <mwu@mozilla.com>
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

// Check that reading non existant inner jars results in the right error

function run_test() {
  var file = do_get_file("data/test_bug597702.zip");
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var outerJarBase = "jar:" + ios.newFileURI(file).spec + "!/";
  var goodSpec = "jar:" + outerJarBase + "inner.jar!/hello";
  var badSpec = "jar:" + outerJarBase + "jar_that_isnt_in_the.jar!/hello";
  var goodChannel = ios.newChannel(goodSpec, null, null);
  var badChannel = ios.newChannel(badSpec, null, null);

  try {
    instr = goodChannel.open();
  } catch (e) {
    do_throw("Failed to open file in inner jar");
  }

  try {
    instr = badChannel.open();
    do_throw("Failed to report that file doesn't exist");
  } catch (e) {
    do_check_true(e.name == "NS_ERROR_FILE_NOT_FOUND");
  }
}

