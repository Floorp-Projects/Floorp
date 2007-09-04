function run_test() {
  var dns = Components.classes["@mozilla.org/network/dns-service;1"]
                      .getService(Components.interfaces.nsIDNSService);
  var rec = dns.resolve("localhost", 0);
  do_check_eq(rec.getNextAddrAsString(), "127.0.0.1");
}
