function run_test() {
  check_localhost();
  check_local_redirect();
}

function check_localhost() {
  var dns = Components.classes["@mozilla.org/network/dns-service;1"]
                      .getService(Components.interfaces.nsIDNSService);
  var rec = dns.resolve("localhost", 0);
  var answer = rec.getNextAddrAsString();
  do_check_true(answer == "127.0.0.1" || answer == "::1");
}

function check_local_redirect() {
  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  prefs.setCharPref("network.dns.localDomains", "local.vingtetun.org");

  var dns = Components.classes["@mozilla.org/network/dns-service;1"]
                      .getService(Components.interfaces.nsIDNSService);
  var rec = dns.resolve("local.vingtetun.org", 0);
  var answer = rec.getNextAddrAsString();
  do_check_true(answer == "127.0.0.1" || answer == "::1");

  prefs.clearUserPref("network.dns.localDomains");
}

