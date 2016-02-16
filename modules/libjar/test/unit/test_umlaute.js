
var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test() {
  var dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties); 
  var tmpDir = dirService.get("TmpD", Ci.nsIFile); 

  var zipfile = do_get_file("data/test_umlaute.zip");

  var testFile = tmpDir.clone();
  testFile.append("test_\u00FC.txt");
  if (testFile.exists()) {
    testFile.remove(false);
  }

  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);
  zipreader.open(zipfile);

  var entries = zipreader.findEntries(null);
  do_check_true(entries.hasMore()); 

  var entryName = entries.getNext();
  do_check_eq(entryName, "test_\u00FC.txt");

  do_check_true(zipreader.hasEntry(entryName));

  var target = tmpDir.clone();
  target.append(entryName);
  target.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0o640); // 0640

  zipreader.extract(entryName, target);

  var entry = zipreader.getEntry(entryName);
  do_check_true(entry != null);

  zipreader.test(entryName);

  zipreader.close();
}
