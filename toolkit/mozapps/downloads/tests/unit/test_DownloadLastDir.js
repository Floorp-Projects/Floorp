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
 * The Original Code is DownloadUtils Test Code.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari <ehsan.akhgari@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

function run_test()
{
  let Cc = Components.classes;
  let Ci = Components.interfaces;
  let Cu = Components.utils;
  Cu.import("resource://gre/modules/DownloadLastDir.jsm");

  do_check_eq(typeof gDownloadLastDir, "object");
  do_check_eq(gDownloadLastDir.file, null);

  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  let tmpDir = dirSvc.get("TmpD", Ci.nsILocalFile);

  gDownloadLastDir.file = tmpDir;
  do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
  do_check_neq(gDownloadLastDir.file, tmpDir);

  gDownloadLastDir.file = 1; // not an nsIFile
  do_check_eq(gDownloadLastDir.file, null);
  gDownloadLastDir.file = tmpDir;

  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  pb.privateBrowsingEnabled = true;
  do_check_eq(gDownloadLastDir.file, null);

  pb.privateBrowsingEnabled = false;
  do_check_eq(gDownloadLastDir.file, null);
  pb.privateBrowsingEnabled = true;

  gDownloadLastDir.file = tmpDir;
  do_check_eq(gDownloadLastDir.file.path, tmpDir.path);
  do_check_neq(gDownloadLastDir.file, tmpDir);

  pb.privateBrowsingEnabled = false;
  do_check_eq(gDownloadLastDir.file, null);
}
