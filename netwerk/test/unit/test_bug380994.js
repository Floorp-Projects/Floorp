/* check resource: protocol for traversal problems */

const specs = [
  "resource:///chrome/../plugins",
  "resource:///chrome%2f../plugins",
  "resource:///chrome/..%2fplugins",
  "resource:///chrome%2f%2e%2e%2fplugins",
  "resource:///../../../..",
  "resource:///..%2f..%2f..%2f..",
  "resource:///%2e%2e"
];

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);

  for each (spec in specs) {
    var uri = ios.newURI(spec, null, null);
    if (uri.spec.indexOf("..") != -1)
      do_throw("resource: traversal remains: '"+spec+"' ==> '"+uri.spec+"'");
  }
}
