function run_test() {
  var stream = Cc["@mozilla.org/io/string-input-stream;1"].
    createInstance(Ci.nsISupportsCString);
  stream.data = "foo bar baz";

  var pump = Cc["@mozilla.org/network/input-stream-pump;1"].
    createInstance(Ci.nsIInputStreamPump);
  pump.init(stream, -1, -1, 0, 0, false);

  // When we pass a null listener argument too asyncRead we expect it to throw
  // instead of crashing.
  try {
    pump.asyncRead(null, null);
  }
  catch (e) {
    return;
  }

  do_throw("asyncRead didn't throw when passed a null listener argument.");
}
