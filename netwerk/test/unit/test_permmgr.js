// tests nsIPermissionManager

var hosts = [
  // format: [host, type, permission]
  ["http://mozilla.org", "cookie", 1],
  ["http://mozilla.org", "image", 2],
  ["http://mozilla.org", "popup", 3],
  ["http://mozilla.com", "cookie", 1],
  ["http://www.mozilla.com", "cookie", 2],
  ["http://dev.mozilla.com", "cookie", 3],
];

var results = [
  // format: [host, type, testPermission result, testExactPermission result]
  // test defaults
  ["http://localhost", "cookie", 0, 0],
  ["http://spreadfirefox.com", "cookie", 0, 0],
  // test different types
  ["http://mozilla.org", "cookie", 1, 1],
  ["http://mozilla.org", "image", 2, 2],
  ["http://mozilla.org", "popup", 3, 3],
  // test subdomains
  ["http://www.mozilla.org", "cookie", 1, 0],
  ["http://www.dev.mozilla.org", "cookie", 1, 0],
  // test different permissions on subdomains
  ["http://mozilla.com", "cookie", 1, 1],
  ["http://www.mozilla.com", "cookie", 2, 2],
  ["http://dev.mozilla.com", "cookie", 3, 3],
  ["http://www.dev.mozilla.com", "cookie", 3, 0],
];

function run_test() {
  Services.prefs.setCharPref("permissions.manager.defaultsUrl", "");
  var pm = Cc["@mozilla.org/permissionmanager;1"].getService(
    Ci.nsIPermissionManager
  );

  var ioService = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );

  var secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );

  // nsIPermissionManager implementation is an extension; don't fail if it's not there
  if (!pm) {
    return;
  }

  // put a few hosts in
  for (var i = 0; i < hosts.length; ++i) {
    let uri = ioService.newURI(hosts[i][0]);
    let principal = secMan.createContentPrincipal(uri, {});

    pm.addFromPrincipal(principal, hosts[i][1], hosts[i][2]);
  }

  // test the result
  for (var i = 0; i < results.length; ++i) {
    let uri = ioService.newURI(results[i][0]);
    let principal = secMan.createContentPrincipal(uri, {});

    Assert.equal(
      pm.testPermissionFromPrincipal(principal, results[i][1]),
      results[i][2]
    );
    Assert.equal(
      pm.testExactPermissionFromPrincipal(principal, results[i][1]),
      results[i][3]
    );
  }

  // test the enumerator ...
  var perms = Array.from(pm.enumerator);
  Assert.equal(perms.length, hosts.length);

  // ... remove all the hosts ...
  for (var j = 0; j < perms.length; ++j) {
    pm.removePermission(perms[j]);
  }

  // ... ensure each and every element is equal ...
  for (var i = 0; i < hosts.length; ++i) {
    for (var j = 0; j < perms.length; ++j) {
      if (
        perms[j].matchesURI(ioService.newURI(hosts[i][0]), true) &&
        hosts[i][1] == perms[j].type &&
        hosts[i][2] == perms[j].capability
      ) {
        perms.splice(j, 1);
        break;
      }
    }
  }
  Assert.equal(perms.length, 0);

  // ... and check the permmgr's empty
  Assert.equal(pm.enumerator.hasMoreElements(), false);

  // test UTF8 normalization behavior: expect ASCII/ACE host encodings
  var utf8 = "b\u00FCcher.dolske.org"; // "bücher.dolske.org"
  var aceref = "xn--bcher-kva.dolske.org";
  var uri = ioService.newURI("http://" + utf8);
  pm.add(uri, "utf8", 1);
  var enumerator = pm.enumerator;
  Assert.equal(enumerator.hasMoreElements(), true);
  var ace = enumerator.getNext().QueryInterface(Ci.nsIPermission);
  Assert.equal(ace.principal.URI.asciiHost, aceref);
  Assert.equal(enumerator.hasMoreElements(), false);

  // test removeAll()
  pm.removeAll();
  Assert.equal(pm.enumerator.hasMoreElements(), false);

  uri = ioService.newURI("https://www.example.com");
  pm.add(uri, "offline-app", pm.ALLOW_ACTION);
  let principal = secMan.createContentPrincipalFromOrigin(
    "https://www.example.com"
  );
  // Remove existing entry.
  let perm = pm.getPermissionObject(principal, "offline-app", true);
  pm.removePermission(perm);
  // Try to remove already deleted entry.
  perm = pm.getPermissionObject(principal, "offline-app", true);
  pm.removePermission(perm);
  Assert.equal(pm.enumerator.hasMoreElements(), false);
}
