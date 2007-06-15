const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  var tld =
    Cc["@mozilla.org/network/effective-tld-service;1"].
    getService(Ci.nsIEffectiveTLDService);

  do_check_eq(tld.getEffectiveTLDLength("localhost"), 9);
  do_check_eq(tld.getEffectiveTLDLength("localhost."), 10);
  do_check_eq(tld.getEffectiveTLDLength("domain.com"), 3);
  do_check_eq(tld.getEffectiveTLDLength("domain.com."), 4);
  do_check_eq(tld.getEffectiveTLDLength("domain.co.uk"), 5);
  do_check_eq(tld.getEffectiveTLDLength("domain.co.uk."), 6);
}
