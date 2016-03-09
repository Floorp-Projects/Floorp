/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Regression test ensuring that that a STORED entry with differing compressed
// and uncompressed sizes is considered to be corrupt.
function run_test() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  var file = do_get_file("data/test_corrupt3.zip");

  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  zipreader.open(file);

  var entries = zipreader.findEntries('*');
  var failed = false;

  while (entries.hasMore()) {
    let entryPath = entries.getNext();
    let entry = zipreader.getEntry(entryPath);
    if (!entry.isDirectory) {
      try {
        let inputStream = zipreader.getInputStream(entryPath);
      } catch (e) {
        failed = true;
      }
    }
  }

  do_check_true(failed);
}

