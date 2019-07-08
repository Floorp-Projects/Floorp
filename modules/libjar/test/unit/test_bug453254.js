function run_test() {
  const zipCache = Cc["@mozilla.org/libjar/zip-reader-cache;1"].createInstance(
    Ci.nsIZipReaderCache
  );
  zipCache.init(1024);
  try {
    zipCache.getZip(null);
    do_throw("Shouldn't get here!");
  } catch (e) {
    if (
      !(e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_INVALID_POINTER)
    ) {
      throw e;
    }
    // do nothing, this test passes
  }
}
