function run_test() {
  const zipCache = Components.classes["@mozilla.org/libjar/zip-reader-cache;1"]
                             .createInstance(Components.interfaces.nsIZipReaderCache);
  zipCache.init(1024);
  try {
    zipCache.getZip(null);
    do_throw("Shouldn't get here!");
  } catch (e if ((e instanceof Components.interfaces.nsIException) &&
                 (e.result == Components.results.NS_ERROR_INVALID_POINTER))) {
    // do nothing, this test passes
  }
}
