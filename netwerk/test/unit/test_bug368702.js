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
  do_check_eq(tld.getPublicSuffixFromHost("co.uk"), "co.uk");
  do_check_eq(tld.getBaseDomainFromHost("domain.co.uk"), "domain.co.uk");
  do_check_eq(tld.getBaseDomainFromHost("domain.co.uk."), "domain.co.uk.");

  try {
    etld = tld.getPublicSuffixFromHost("");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    etld = tld.getBaseDomainFromHost("domain.co.uk", 1);
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    etld = tld.getBaseDomainFromHost("co.uk");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    etld = tld.getBaseDomainFromHost("");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("1.2.3.4");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("2010:836B:4179::836B:4179");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("3232235878");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("::ffff:192.9.5.5");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("::1");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  // Check IP addresses with trailing dot as well, Necko sometimes accepts
  // those (depending on operating system, see bug 380543)
  try {
    etld = tld.getPublicSuffixFromHost("127.0.0.1.");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    etld = tld.getPublicSuffixFromHost("::ffff:127.0.0.1.");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  // check normalization: output should be consistent with
  // nsIURI::GetAsciiHost(), i.e. lowercased and ASCII/ACE encoded
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);

  var uri = ioService.newURI("http://b\u00FCcher.co.uk", null, null);
  do_check_eq(tld.getBaseDomain(uri), "xn--bcher-kva.co.uk");
  do_check_eq(tld.getBaseDomainFromHost("b\u00FCcher.co.uk"), "xn--bcher-kva.co.uk");
  do_check_eq(tld.getPublicSuffix(uri), "co.uk");
  do_check_eq(tld.getPublicSuffixFromHost("b\u00FCcher.co.uk"), "co.uk");

  // check that malformed hosts are rejected as invalid args
  try {
    etld = tld.getBaseDomainFromHost("domain.co.uk..");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    etld = tld.getBaseDomainFromHost("domain.co..uk");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    etld = tld.getBaseDomainFromHost(".domain.co.uk");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    etld = tld.getBaseDomainFromHost(".domain.co.uk");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    etld = tld.getBaseDomainFromHost(".");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    etld = tld.getBaseDomainFromHost("..");
    do_throw("this should fail");
  } catch(e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }
}
