// Test checks SIGBUS handling on Linux. The test cannot be used to check page
// error exception on Windows because the file cannot be truncated while it's
// being used by zipreader.
function run_test() {
  var file = do_get_file("data/test_bug333423.zip");
  var tmpFile = do_get_tempdir();

  file.copyTo(tmpFile, "bug1550815.zip");
  tmpFile.append("bug1550815.zip");

  var zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );
  zipReader.open(tmpFile);

  // Truncate the file
  var ostream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  ostream.init(tmpFile, -1, -1, 0);
  ostream.close();

  try {
    zipReader.test("modules/libjar/test/Makefile.in");
    Assert.ok(false, "Should not reach here.");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_FILE_NOT_FOUND);
  }

  zipReader.close();
  tmpFile.remove(false);
}
