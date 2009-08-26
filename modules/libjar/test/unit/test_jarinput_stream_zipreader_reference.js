/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * Taras Glek <tglek@mozilla.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
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

function wrapInputStream(input)
{
  var nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;
  var factory = Components.classes["@mozilla.org/scriptableinputstream;1"];
  var wrapper = factory.createInstance(nsIScriptableInputStream);
  wrapper.init(input);
  return wrapper;
}

// Check that files can be read from after closing zipreader
function run_test() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  // the build script have created the zip we can test on in the current dir.
  var file = do_get_file("data/test_bug333423.zip");

  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  zipreader.open(file);
  // do crc stuff
  function check_archive_crc() {
    zipreader.test(null);
    return true;
  }
  do_check_true(check_archive_crc())
  var entries = zipreader.findEntries(null);
  var stream = wrapInputStream(zipreader.getInputStream("modules/libjar/test/Makefile.in"))
  var dirstream= wrapInputStream(zipreader.getInputStream("modules/libjar/test/"))
  zipreader.close();
  zipreader = null;
  Components.utils.forceGC();
  do_check_true(stream.read(1024).length > 0);
  do_check_true(dirstream.read(100).length > 0);
}

