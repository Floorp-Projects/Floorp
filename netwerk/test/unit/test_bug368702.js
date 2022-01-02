"use strict";

function run_test() {
  var tld = Services.eTLD;
  Assert.equal(tld.getPublicSuffixFromHost("localhost"), "localhost");
  Assert.equal(tld.getPublicSuffixFromHost("localhost."), "localhost.");
  Assert.equal(tld.getPublicSuffixFromHost("domain.com"), "com");
  Assert.equal(tld.getPublicSuffixFromHost("domain.com."), "com.");
  Assert.equal(tld.getPublicSuffixFromHost("domain.co.uk"), "co.uk");
  Assert.equal(tld.getPublicSuffixFromHost("domain.co.uk."), "co.uk.");
  Assert.equal(tld.getPublicSuffixFromHost("co.uk"), "co.uk");
  Assert.equal(tld.getBaseDomainFromHost("domain.co.uk"), "domain.co.uk");
  Assert.equal(tld.getBaseDomainFromHost("domain.co.uk."), "domain.co.uk.");

  try {
    tld.getPublicSuffixFromHost("");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    tld.getBaseDomainFromHost("domain.co.uk", 1);
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    tld.getBaseDomainFromHost("co.uk");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    tld.getBaseDomainFromHost("");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS);
  }

  try {
    tld.getPublicSuffixFromHost("1.2.3.4");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    tld.getPublicSuffixFromHost("2010:836B:4179::836B:4179");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    tld.getPublicSuffixFromHost("3232235878");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    tld.getPublicSuffixFromHost("::ffff:192.9.5.5");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    tld.getPublicSuffixFromHost("::1");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  // Check IP addresses with trailing dot as well, Necko sometimes accepts
  // those (depending on operating system, see bug 380543)
  try {
    tld.getPublicSuffixFromHost("127.0.0.1.");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  try {
    tld.getPublicSuffixFromHost("::ffff:127.0.0.1.");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_HOST_IS_IP_ADDRESS);
  }

  // check normalization: output should be consistent with
  // nsIURI::GetAsciiHost(), i.e. lowercased and ASCII/ACE encoded
  var uri = Services.io.newURI("http://b\u00FCcher.co.uk");
  Assert.equal(tld.getBaseDomain(uri), "xn--bcher-kva.co.uk");
  Assert.equal(
    tld.getBaseDomainFromHost("b\u00FCcher.co.uk"),
    "xn--bcher-kva.co.uk"
  );
  Assert.equal(tld.getPublicSuffix(uri), "co.uk");
  Assert.equal(tld.getPublicSuffixFromHost("b\u00FCcher.co.uk"), "co.uk");

  // check that malformed hosts are rejected as invalid args
  try {
    tld.getBaseDomainFromHost("domain.co.uk..");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    tld.getBaseDomainFromHost("domain.co..uk");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    tld.getBaseDomainFromHost(".domain.co.uk");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    tld.getBaseDomainFromHost(".domain.co.uk");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    tld.getBaseDomainFromHost(".");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  try {
    tld.getBaseDomainFromHost("..");
    do_throw("this should fail");
  } catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }
}
