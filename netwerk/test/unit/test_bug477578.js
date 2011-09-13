const Cc = Components.classes;
const Ci = Components.interfaces;

const testMethods = [
  ["get", "GET"],
  ["post", "POST"],
  ["head", "HEAD"],
  ["put", "PUT"],
  ["delete", "DELETE"],
  ["connect", "CONNECT"],
  ["options", "OPTIONS"],
  ["trace", "TRACE"],
  ["track", "TRACK"],
  ["copy", "copy"],
  ["index", "index"],
  ["lock", "lock"],
  ["m-post", "m-post"],
  ["mkcol", "mkcol"],
  ["move", "move"],
  ["propfind", "propfind"],
  ["proppatch", "proppatch"],
  ["unlock", "unlock"],
  ["link", "link"],
  ["foo", "foo"],
  ["foO", "foO"],
  ["fOo", "fOo"],
  ["Foo", "Foo"]
]

function run_test() {
  var ios =
    Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var chan = ios.newChannel("http://localhost/", null, null)
                  .QueryInterface(Components.interfaces.nsIHttpChannel);

  for (var i = 0; i < testMethods.length; i++) {
    chan.requestMethod = testMethods[i][0];
    do_check_eq(chan.requestMethod, testMethods[i][1]);
  }
}
