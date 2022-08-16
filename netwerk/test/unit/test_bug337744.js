/* verify that certain invalid URIs are not parsed by the resource
   protocol handler */

"use strict";

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
var uri = NetUtil.newURI("http://www.example.com");
var principal = Services.scriptSecurityManager.createContentPrincipal(uri, {});

function get_channel(spec) {
  var channel = NetUtil.newChannel({
    uri: NetUtil.newURI(spec),
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });

  Assert.throws(
    () => {
      channel.asyncOpen(null);
    },
    /NS_ERROR_DOM_BAD_URI/,
    `asyncOpen() of uri: ${spec} should throw`
  );
  Assert.throws(
    () => {
      channel.open();
    },
    /NS_ERROR_DOM_BAD_URI/,
    `Open() of uri: ${spec} should throw`
  );

  return channel;
}

function check_safe_resolution(spec, rootURI) {
  info(`Testing URL "${spec}"`);

  let channel = get_channel(spec);

  ok(
    channel.name.startsWith(rootURI),
    `URL resolved safely to ${channel.name}`
  );
  let startOfQuery = channel.name.indexOf("?");
  if (startOfQuery == -1) {
    ok(!/%2f/i.test(channel.name), `URL contains no escaped / characters`);
  } else {
    // Escaped slashes are allowed in the query or hash part of the URL
    ok(
      !channel.name.replace(/\?.*/, "").includes("%2f"),
      `URL contains no escaped slashes before the query ${channel.name}`
    );
  }
}

function check_resolution_error(spec) {
  Assert.throws(
    () => {
      get_channel(spec);
    },
    /NS_ERROR_MALFORMED_URI/,
    "Expected a malformed URI error"
  );
}

function run_test() {
  // resource:/// and resource://gre/ are resolved specially, so we need
  // to create a temporary resource package to test the standard logic
  // with.

  let resProto = Cc["@mozilla.org/network/protocol;1?name=resource"].getService(
    Ci.nsIResProtocolHandler
  );
  let rootFile = Services.dirsvc.get("GreD", Ci.nsIFile);
  let rootURI = Services.io.newFileURI(rootFile);

  rootFile.append("directory-that-does-not-exist");
  let inexistentURI = Services.io.newFileURI(rootFile);

  resProto.setSubstitution("res-test", rootURI);
  resProto.setSubstitution("res-inexistent", inexistentURI);
  registerCleanupFunction(() => {
    resProto.setSubstitution("res-test", null);
    resProto.setSubstitution("res-inexistent", null);
  });

  let baseRoot = resProto.resolveURI(Services.io.newURI("resource:///"));
  let greRoot = resProto.resolveURI(Services.io.newURI("resource://gre/"));

  for (let spec of specs) {
    check_safe_resolution(spec, rootURI.spec);
    check_safe_resolution(
      spec.replace("res-test", "res-inexistent"),
      inexistentURI.spec
    );
    check_safe_resolution(spec.replace("res-test", ""), baseRoot);
    check_safe_resolution(spec.replace("res-test", "gre"), greRoot);
  }

  for (let spec of error_specs) {
    check_resolution_error(spec);
  }
}
