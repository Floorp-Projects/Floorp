// tests nsIPermissionManager

var hosts = [
  // format: [host, type, permission]
  ["mozilla.org", "cookie", 1],
  ["mozilla.org", "image", 2],
  ["mozilla.org", "popup", 3],
  ["mozilla.com", "cookie", 1],
  ["www.mozilla.com", "cookie", 2],
  ["dev.mozilla.com", "cookie", 3]
];

var results = [
  // format: [host, type, testPermission result, testExactPermission result]
  // test defaults
  ["localhost", "cookie", 0, 0],
  ["spreadfirefox.com", "cookie", 0, 0],
  // test different types
  ["mozilla.org", "cookie", 1, 1],
  ["mozilla.org", "image", 2, 2],
  ["mozilla.org", "popup", 3, 3],
  // test subdomains
  ["www.mozilla.org", "cookie", 1, 0],
  ["www.dev.mozilla.org", "cookie", 1, 0],
  // test different permissions on subdomains
  ["mozilla.com", "cookie", 1, 1],
  ["www.mozilla.com", "cookie", 2, 2],
  ["dev.mozilla.com", "cookie", 3, 3],
  ["www.dev.mozilla.com", "cookie", 3, 0]
];

function run_test() {
  var pm = Components.classes["@mozilla.org/permissionmanager;1"]
                     .getService(Components.interfaces.nsIPermissionManager);

  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);

  var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(Components.interfaces.nsIScriptSecurityManager);

  // nsIPermissionManager implementation is an extension; don't fail if it's not there
  if (!pm)
    return;

  // put a few hosts in
  for (var i = 0; i < hosts.length; ++i) {
    var uri = ioService.newURI("http://" + hosts[i][0], null, null);
    var principal = secMan.getNoAppCodebasePrincipal(uri);

    pm.addFromPrincipal(principal, hosts[i][1], hosts[i][2]);
  }

  // test the result
  for (var i = 0; i < results.length; ++i) {
    var uri = ioService.newURI("http://" + results[i][0], null, null);
    var principal = secMan.getNoAppCodebasePrincipal(uri);

    do_check_eq(pm.testPermissionFromPrincipal(principal, results[i][1]), results[i][2]);
    do_check_eq(pm.testExactPermissionFromPrincipal(principal, results[i][1]), results[i][3]);
  }

  // test the enumerator ...
  var j = 0;
  var perms = new Array();
  var enumerator = pm.enumerator;
  while (enumerator.hasMoreElements()) {
    perms[j] = enumerator.getNext().QueryInterface(Components.interfaces.nsIPermission);
    ++j;
  }
  do_check_eq(perms.length, hosts.length);

  // ... remove all the hosts ...
  for (var j = 0; j < perms.length; ++j) {
    var uri = ioService.newURI("http://" + perms[j].host, null, null);
    var principal = secMan.getNoAppCodebasePrincipal(uri);

    pm.removeFromPrincipal(principal, perms[j].type);
  }
  
  // ... ensure each and every element is equal ...
  for (var i = 0; i < hosts.length; ++i) {
    for (var j = 0; j < perms.length; ++j) {
      if (hosts[i][0] == perms[j].host &&
          hosts[i][1] == perms[j].type &&
          hosts[i][2] == perms[j].capability) {
        perms.splice(j, 1);
        break;
      }
    }
  }
  do_check_eq(perms.length, 0);

  // ... and check the permmgr's empty
  do_check_eq(pm.enumerator.hasMoreElements(), false);

  // test UTF8 normalization behavior: expect ASCII/ACE host encodings
  var utf8 = "b\u00FCcher.dolske.org"; // "bücher.dolske.org"
  var aceref = "xn--bcher-kva.dolske.org";
  var uri = ioService.newURI("http://" + utf8, null, null);
  pm.add(uri, "utf8", 1);
  var enumerator = pm.enumerator;
  do_check_eq(enumerator.hasMoreElements(), true);
  var ace = enumerator.getNext().QueryInterface(Components.interfaces.nsIPermission);
  do_check_eq(ace.host, aceref);
  do_check_eq(enumerator.hasMoreElements(), false);

  // test removeAll()
  pm.removeAll();
  do_check_eq(pm.enumerator.hasMoreElements(), false);
}
