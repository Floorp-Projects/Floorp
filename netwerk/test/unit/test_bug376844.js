const testURLs = [
  ["http://example.com/<", "http://example.com/%3C"],
  ["http://example.com/>", "http://example.com/%3E"],
  ["http://example.com/'", "http://example.com/%27"],
  ["http://example.com/\"", "http://example.com/%22"],
  ["http://example.com/?<", "http://example.com/?%3C"],
  ["http://example.com/?>", "http://example.com/?%3E"],
  ["http://example.com/?'", "http://example.com/?%27"],
  ["http://example.com/?\"", "http://example.com/?%22"]
]

function run_test() {
  var ioServ =
    Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  for (var i = 0; i < testURLs.length; i++) {
    var uri = ioServ.newURI(testURLs[i][0], null, null);
    do_check_eq(uri.spec, testURLs[i][1]);
  }
}
