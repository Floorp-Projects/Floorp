/* verify that certain invalid URIs are not parsed by the resource
   protocol handler */

const specs = [
  "resource:////",
  "resource:///http://www.mozilla.org/",
  "resource:///file:///",
  "resource:///..\\",
  "resource:///..\\..\\",
  "resource:///..%5C",
  "resource:///..%5c"
];

function check_for_exception(spec)
{
  var ios =
    Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  try {
    var channel = ios.newChannel2(spec,
                                  null,
                                  null,
                                  null,      // aLoadingNode
                                  Services.scriptSecurityManager.getSystemPrincipal(),
                                  null,      // aTriggeringPrincipal
                                  Ci.nsILoadInfo.SEC_NORMAL,
                                  Ci.nsIContentPolicy.TYPE_OTHER);
  }
  catch (e) {
    return;
  }

  do_throw("Successfully opened invalid URI: '" + spec + "'");
}

function run_test() {
  for (var spec of specs) {
    check_for_exception(spec);
  }
}
