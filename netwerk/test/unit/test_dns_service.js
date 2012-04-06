function run_test() {
  var dns = Components.classes["@mozilla.org/network/dns-service;1"]
                      .getService(Components.interfaces.nsIDNSService);
  var rec = dns.resolve("localhost", 0);
  var answer = rec.getNextAddrAsString();
  do_check_true(answer == "127.0.0.1" || answer == "::1");
}
