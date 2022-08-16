// tests nsIPermissionManager

"use strict";

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
  var pm = Services.perms;

  var ioService = Services.io;

  var secMan = Services.scriptSecurityManager;

  // nsIPermissionManager implementation is an extension; don't fail if it's not there
  if (!pm) {
    return;
  }

  // put a few hosts in
  for (let i = 0; i < hosts.length; ++i) {
    let uri = ioService.newURI(hosts[i][0]);
    let principal = secMan.createContentPrincipal(uri, {});

    pm.addFromPrincipal(principal, hosts[i][1], hosts[i][2]);
  }

  // test the result
  for (let i = 0; i < results.length; ++i) {
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

  // test the all property ...
  var perms = pm.all;
  Assert.equal(perms.length, hosts.length);

  // ... remove all the hosts ...
  for (let j = 0; j < perms.length; ++j) {
    pm.removePermission(perms[j]);
  }

  // ... ensure each and every element is equal ...
  for (let i = 0; i < hosts.length; ++i) {
    for (let j = 0; j < perms.length; ++j) {
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
  Assert.equal(pm.all.length, 0);

  // test UTF8 normalization behavior: expect ASCII/ACE host encodings
  var utf8 = "b\u00FCcher.dolske.org"; // "bücher.dolske.org"
  var aceref = "xn--bcher-kva.dolske.org";
  var principal = secMan.createContentPrincipal(
    ioService.newURI("http://" + utf8),
    {}
  );
  pm.addFromPrincipal(principal, "utf8", 1);
  Assert.notEqual(Services.perms.all.length, 0);
  var ace = Services.perms.all[0];
  Assert.equal(ace.principal.asciiHost, aceref);
  Assert.equal(Services.perms.all.length > 1, false);

  // test removeAll()
  pm.removeAll();
  Assert.equal(Services.perms.all.length, 0);

  principal = secMan.createContentPrincipalFromOrigin(
    "https://www.example.com"
  );
  pm.addFromPrincipal(principal, "offline-app", pm.ALLOW_ACTION);
  // Remove existing entry.
  let perm = pm.getPermissionObject(principal, "offline-app", true);
  pm.removePermission(perm);
  // Try to remove already deleted entry.
  perm = pm.getPermissionObject(principal, "offline-app", true);
  pm.removePermission(perm);
  Assert.equal(Services.perms.all.length, 0);
}
