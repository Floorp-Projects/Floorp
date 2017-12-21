/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  var file = do_get_file("data/test_corrupt.zip");

  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  zipreader.open(file);
  //  var entries = zipreader.findEntries(null);
  // the signature for file is corrupt, should not segfault
  var failed = false;
  try {
    var stream = wrapInputStream(zipreader.getInputStream("file"));
    stream.read(1024);
  } catch (ex) {
    failed = true;
  }
  Assert.ok(failed);
}

