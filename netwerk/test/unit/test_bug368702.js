const Cc = Components.classes;
const Ci = Components.interfaces;

const NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS = 0x804b0050;
const NS_ERROR_HOST_IS_IP_ADDRESS = 0x804b0051;

function run_test() {
  var tld =
    Cc["@mozilla.org/network/effective-tld-service;1"].
    getService(Ci.nsIEffectiveTLDService);

  var etld;

  do_check_eq(tld.getPublicSuffixFromHost("localhost"), "localhost");
  do_check_eq(tld.getPublicSuffixFromHost("localhost."), "localhost.");
  do_check_eq(tld.getPublicSuffixFromHost("domain.com"), "com");
  do_check_eq(tld.getPublicSuffixFromHost("domain.com."), "com.");
  do_check_eq(tld.getPublicSuffixFromHost("domain.co.uk"), "co.uk");
  do_check_eq(tld.getPublicSuffixFromHost("domain.co.uk."), "co.uk.");
  do_check_eq(tld.getBaseDomainFromHost("domain.co.uk"), "domain.co.uk");
  do_check_eq(tld.getBaseDomainFromHost("domain.co.uk."), "domain.co.uk.");

  try {
    etld = tld.getBaseDomainFromHost("domain.co.uk", 1);
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("1.2.3.4");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("2010:836B:4179::836B:4179");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("3232235878");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("::ffff:192.9.5.5");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("::1");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  // Check IP addresses with trailing dot as well, Necko sometimes accepts
  // those (depending on operating system, see bug 380543)
  try {
    etld = tld.getPublicSuffixFromHost("127.0.0.1.");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("::ffff:127.0.0.1.");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, NS_ERROR_HOST_IS_IP_ADDRESS);
  }
}
