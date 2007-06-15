const Cc = Components.classes;
const Ci = Components.interfaces;

const url = "http://foo.com/folder/file?/.";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var newURI = ios.newURI(url, null, null);
  do_check_eq(newURI.spec, url);
  do_check_eq(newURI.path, "/folder/file?/.");
  do_check_eq(newURI.resolve("./file?/."), url);
}
