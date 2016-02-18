/* verify that certain invalid URIs are not parsed by the resource
   protocol handler */

Cu.import("resource://gre/modules/NetUtil.jsm");

const specs = [
  "resource:////",
  "resource:///http://www.mozilla.org/",
  "resource:///file:///",
  "resource:///..\\",
  "resource:///..\\..\\",
  "resource:///..%5C",
  "resource:///..%5c"
];

var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"]
            .getService(Ci.nsIScriptSecurityManager);
// create some fake principal that has not engough
// privileges to access any resource: uri.
var uri = NetUtil.newURI("http://www.example.com", null, null);
var principal = ssm.createCodebasePrincipal(uri, {});

function check_for_exception(spec)
{
  var channelURI = NetUtil.newURI(spec, null, null);

  var channel = NetUtil.newChannel({
    uri: NetUtil.newURI(spec, null, null),
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER
  });

  try {
    channel.asyncOpen2(null);
    do_check_true(false, "asyncOpen2() of URI: " + spec + "should throw");
  }
  catch (e) {
    // make sure we get the right error code in the exception
    // ERROR code for NS_ERROR_DOM_BAD_URI is 1012
    do_check_eq(e.code, 1012);
  }

  try {
    channel.open2();
    do_check_true(false, "Open2() of uri: " + spec + "should throw");
  }
  catch (e) {
    // make sure we get the right error code in the exception
    // ERROR code for NS_ERROR_DOM_BAD_URI is 1012
    do_check_eq(e.code, 1012);
  }
}

function run_test() {
  for (var spec of specs) {
    check_for_exception(spec);
  }
}
