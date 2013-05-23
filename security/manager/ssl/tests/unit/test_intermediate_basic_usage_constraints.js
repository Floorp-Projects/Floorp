"use strict";
/* To regenerate the certificates for this test:
 *
 *      cd security/manager/ssl/tests/unit/test_certificate_usages
 *       ./generate.py
 *      cd ../../../../../..
 *      make -C $OBJDIR/security/manager/ssl/tests
 *
 * Check in the generated files. These steps are not done as part of the build
 * because we do not want to add a build-time dependency on the OpenSSL or NSS
 * tools or libraries built for the host platform.
 */

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

const ca_usage1 = 'Client,Server,Sign,Encrypt,SSL CA,Status Responder';
const ee_usage1 = 'Client,Server,Sign,Encrypt';
const ee_usage2 = 'Client,Server,Sign,Encrypt,Status Responder';

const cert2usage = {
  'int-no-extensions': ee_usage1,
  'ee-int-no-extensions': "",

  'int-not-a-ca': ee_usage1,
  'ee-int-not-a-ca': "",

  'int-limited-depth' : ca_usage1,
  'ee-int-limited-depth' : ee_usage1,

  'int-limited-depth-invalid' : ca_usage1, // XXX this should be ''
  'ee-int-limited-depth-invalid' : "",

  'int-valid-ku-no-eku' : 'SSL CA',
  'ee-int-valid-ku-no-eku' : ee_usage1,

  'int-bad-ku-no-eku' : ee_usage2,
  'ee-int-bad-ku-no-eku' : "",

  'int-no-ku-no-eku' : ca_usage1,
  'ee-int-no-ku-no-eku' : ee_usage1,

  'int-valid-ku-server-eku' : 'SSL CA',
  'ee-int-valid-ku-server-eku' : "",

  'int-bad-ku-server-eku' : '',
  'ee-int-bad-ku-server-eku' : "",

  'int-no-ku-server-eku' : 'SSL CA',
  'ee-int-no-ku-server-eku' : ""
};

function load_cert(name, trust) {
  let filename = "test_intermediate_basic_usage_constraints/" + name + ".der";
  addCertFromFile(certdb, filename, trust);
}

function test_cert_for_usages(cert_nick, expected_usages_string) {
  let cert = certdb.findCertByNickname(null, cert_nick);
  let verified = {};
  let usages = {};
  cert.getUsagesString(true, verified, usages);
  do_print("usages.value = " + usages.value);
  do_check_eq(expected_usages_string, usages.value);
}

function run_test() {
  // Load the ca into mem
  let ca_name = "ca";
  load_cert(ca_name, "CTu,CTu,CTu");
  do_print("ca_name = " + ca_name);
  test_cert_for_usages(ca_name, ca_usage1);

  // Load certs first
  for (let cert_name in cert2usage) {
    load_cert(cert_name, ',,');
  }

  // Now do the checks
  for (let cert_name in cert2usage) {
    do_print("cert_name =" + cert_name);
    test_cert_for_usages(cert_name, cert2usage[cert_name]);
  }
}
