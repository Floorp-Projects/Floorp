function run_test() {
  var tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);

  var zipfile = do_get_file("data/test_umlaute.zip");

  var testFile = tmpDir.clone();
  testFile.append("test_\u00FC.txt");
  if (testFile.exists()) {
    testFile.remove(false);
  }

  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );
  zipreader.open(zipfile);

  var entries = zipreader.findEntries(null);
  Assert.ok(entries.hasMore());

  var entryName = entries.getNext();
  Assert.equal(entryName, "test_\u00FC.txt");

  Assert.ok(zipreader.hasEntry(entryName));

  var target = tmpDir.clone();
  target.append(entryName);
  target.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o640); // 0640

  zipreader.extract(entryName, target);

  var entry = zipreader.getEntry(entryName);
  Assert.ok(entry != null);

  zipreader.test(entryName);

  zipreader.close();
}
