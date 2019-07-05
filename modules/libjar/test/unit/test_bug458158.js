function run_test() {
  var zReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );
  try {
    zReader.open(null);
    do_throw("Shouldn't get here!");
  } catch (e) {
    if (
      !(e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_NULL_POINTER)
    ) {
      throw e;
    }
    // do nothing, this test passes
  }
}
