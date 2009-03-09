function run_test() {
  var zReader = Components.classes["@mozilla.org/libjar/zip-reader;1"]
                        .createInstance(Components.interfaces.nsIZipReader);
  try {
    zReader.open(null);
    do_throw("Shouldn't get here!");
  } catch (e if (e instanceof Components.interfaces.nsIException &&
                 e.result == Components.results.NS_ERROR_NULL_POINTER)) {
    // do nothing, this test passes
  }
}
