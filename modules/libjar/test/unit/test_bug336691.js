// Regression test for bug 336691 - nsZipArchive::Test shouldn't try to ExtractFile on directories.
var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test() {
  var file = do_get_file("data/test_bug336691.zip");
  var zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  zipReader.open(file);
  zipReader.test(null); // We shouldn't crash here.
  zipReader.close();
}
