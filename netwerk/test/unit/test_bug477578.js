// test that methods are not normalized

Cu.import("resource://gre/modules/Services.jsm");

const testMethods = [
  ["GET"],
  ["get"],
  ["Get"],
  ["gET"],
  ["gEt"],
  ["post"],
  ["POST"],
  ["head"],
  ["HEAD"],
  ["put"],
  ["PUT"],
  ["delete"],
  ["DELETE"],
  ["connect"],
  ["CONNECT"],
  ["options"],
  ["trace"],
  ["track"],
  ["copy"],
  ["index"],
  ["lock"],
  ["m-post"],
  ["mkcol"],
  ["move"],
  ["propfind"],
  ["proppatch"],
  ["unlock"],
  ["link"],
  ["LINK"],
  ["foo"],
  ["foO"],
  ["fOo"],
  ["Foo"]
]

function run_test() {
  var ios =
    Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var chan = ios.newChannel2("http://localhost/",
                             null,
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER)
                  .QueryInterface(Components.interfaces.nsIHttpChannel);

  for (var i = 0; i < testMethods.length; i++) {
    chan.requestMethod = testMethods[i];
    do_check_eq(chan.requestMethod, testMethods[i]);
  }
}
