/* verify that certain invalid URIs are not parsed by the resource
   protocol handler */

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const specs = [
  "resource://res-test//",
  "resource://res-test/?foo=http:",
  "resource://res-test/?foo=" + encodeURIComponent("http://example.com/"),
  "resource://res-test/?foo=" + encodeURIComponent("x\\y"),
  "resource://res-test/..%2F",
  "resource://res-test/..%2f",
  "resource://res-test/..%2F..",
  "resource://res-test/..%2f..",
  "resource://res-test/../../",
  "resource://res-test/http://www.mozilla.org/",
  "resource://res-test/file:///",
];

const error_specs = [
  "resource://res-test/..\\",
  "resource://res-test/..\\..\\",
  "resource://res-test/..%5C",
  "resource://res-test/..%5c",
];

// Create some fake principal that has not enough
// privileges to access any resource: uri.
var uri = NetUtil.newURI("http://www.example.com", null, null);
var principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});

function get_channel(spec)
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
    ok(false, "asyncOpen2() of URI: " + spec + "should throw");
  }
  catch (e) {
    // make sure we get the right error code in the exception
    // ERROR code for NS_ERROR_DOM_BAD_URI is 1012
    equal(e.code, 1012);
  }

  try {
    channel.open2();
    ok(false, "Open2() of uri: " + spec + "should throw");
  }
  catch (e) {
    // make sure we get the right error code in the exception
    // ERROR code for NS_ERROR_DOM_BAD_URI is 1012
    equal(e.code, 1012);
  }

  return channel;
}

function check_safe_resolution(spec, rootURI)
{
  do_print(`Testing URL "${spec}"`);

  let channel = get_channel(spec);

  ok(channel.name.startsWith(rootURI), `URL resolved safely to ${channel.name}`);
  ok(!/%2f/i.test(channel.name), `URL contains no escaped / characters`);
}

function check_resolution_error(spec)
{
  try {
    get_channel(spec);
    ok(false, "Expected an error");
  } catch (e) {
    equal(e.result, Components.results.NS_ERROR_MALFORMED_URI,
          "Expected a malformed URI error");
  }
}

function run_test() {
  // resource:/// and resource://gre/ are resolved specially, so we need
  // to create a temporary resource package to test the standard logic
  // with.

  let resProto = Cc['@mozilla.org/network/protocol;1?name=resource'].getService(Ci.nsIResProtocolHandler);
  let rootFile = Services.dirsvc.get("GreD", Ci.nsIFile);
  let rootURI = Services.io.newFileURI(rootFile);

  resProto.setSubstitution("res-test", rootURI);
  do_register_cleanup(() => {
    resProto.setSubstitution("res-test", null);
  });

  let baseRoot = resProto.resolveURI(Services.io.newURI("resource:///", null, null));
  let greRoot = resProto.resolveURI(Services.io.newURI("resource://gre/", null, null));

  for (var spec of specs) {
    check_safe_resolution(spec, rootURI.spec);
    check_safe_resolution(spec.replace("res-test", ""), baseRoot);
    check_safe_resolution(spec.replace("res-test", "gre"), greRoot);
  }

  for (var spec of error_specs) {
    check_resolution_error(spec);
  }
}
