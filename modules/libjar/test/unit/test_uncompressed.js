// Make sure uncompressed files pass crc
const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  var file = do_get_file("data/uncompressed.zip");
  var zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  zipReader.open(file);
  zipReader.test("hello");
  zipReader.close();
}
