const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var uri1 = ios.newURI("http://example.com#bar", null, null);
  var uri2 = ios.newURI("http://example.com/#bar", null, null);
  do_check_true(uri1.equals(uri2));

  uri1.spec = "http://example.com?bar";
  uri2.spec = "http://example.com/?bar";
  do_check_true(uri1.equals(uri2));

  // see https://bugzilla.mozilla.org/show_bug.cgi?id=665706
  // ";" is not parsed as special anymore and thus ends up
  // in the authority component (see RFC 3986)
  uri1.spec = "http://example.com;bar";
  uri2.spec = "http://example.com/;bar";
  do_check_false(uri1.equals(uri2));

  uri1.spec = "http://example.com#";
  uri2.spec = "http://example.com/#";
  do_check_true(uri1.equals(uri2));

  uri1.spec = "http://example.com?";
  uri2.spec = "http://example.com/?";
  do_check_true(uri1.equals(uri2));

  // see https://bugzilla.mozilla.org/show_bug.cgi?id=665706
  // ";" is not parsed as special anymore and thus ends up
  // in the authority component (see RFC 3986)
  uri1.spec = "http://example.com;";
  uri2.spec = "http://example.com/;";
  do_check_false(uri1.equals(uri2));

  uri1.spec = "http://example.com";
  uri2.spec = "http://example.com/";
  do_check_true(uri1.equals(uri2));
}
