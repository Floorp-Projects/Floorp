const Cc = Components.classes;
const Ci = Components.interfaces;

function test_not_too_long() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var spec = "jar:http://example.com/bar.jar!/";
  try {
    var newURI = ios.newURI(spec, null, null);
  }
  catch (e) {
    do_throw("newURI threw even though it wasn't passed a large nested URI?");
  }
}

function test_too_long() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var i;
  var prefix = "jar:";
  for (i = 0; i < 16; i++) {
    prefix = prefix + prefix;
  }
  var suffix = "!/";
  for (i = 0; i < 16; i++) {
    suffix = suffix + suffix;
  }

  var spec = prefix + "http://example.com/bar.jar" + suffix;
  try {
    // The following will produce a recursive call that if
    // unchecked would lead to a stack overflow. If we
    // do not crash here and thus an exception is caught
    // we have passed the test.
    var newURI = ios.newURI(spec, null, null);
  }
  catch (e) {
    return;
  }
}

function run_test() {
  test_not_too_long();
  test_too_long();
}
