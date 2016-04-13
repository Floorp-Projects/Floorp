/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function wrapInputStream(input)
{
  let nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;
  let factory = Components.classes["@mozilla.org/scriptableinputstream;1"];
  let wrapper = factory.createInstance(nsIScriptableInputStream);
  wrapper.init(input);
  return wrapper;
}

// Make sure that we can read from CRX files as if they were ZIP files.
function run_test() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  // Note: test_crx_dummy.crx is a dummy crx file created for this test. The
  // public key and signature fields in the header are both empty.
  let file = do_get_file("data/test_crx_dummy.crx");

  let zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  zipreader.open(file);
  // do crc stuff
  function check_archive_crc() {
    zipreader.test(null);
    return true;
  }
  Assert.ok(check_archive_crc())
  let entries = zipreader.findEntries(null);
  let stream = wrapInputStream(zipreader.getInputStream("modules/libjar/test/Makefile.in"))
  let dirstream= wrapInputStream(zipreader.getInputStream("modules/libjar/test/"))
  zipreader.close();
  zipreader = null;
  Components.utils.forceGC();
  Assert.ok(stream.read(1024).length > 0);
  Assert.ok(dirstream.read(100).length > 0);
}

