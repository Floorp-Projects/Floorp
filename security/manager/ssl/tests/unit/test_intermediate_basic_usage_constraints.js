"use strict";
/* To regenerate the certificates for this test:
 *
 *      cd security/manager/ssl/tests/unit/test_intermediate_basic_usage_constraints
 *      ./generate.py
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

function load_cert(name, trust) {
  let filename = "test_intermediate_basic_usage_constraints/" + name + ".der";
  addCertFromFile(certdb, filename, trust);
}

function test_cert_for_usages(certChainNicks, expected_usages_string) {
  let certs = [];
  for (let i in certChainNicks) {
    let certNick = certChainNicks[i];
    let certDER = readFile(do_get_file(
                             "test_intermediate_basic_usage_constraints/"
                                + certNick + ".der"), false);
    certs.push(certdb.constructX509(certDER, certDER.length));
  }

  let cert = certs[0];
  let verified = {};
  let usages = {};
  cert.getUsagesString(true, verified, usages);
  do_print("usages.value = " + usages.value);
  do_check_eq(expected_usages_string, usages.value);
}

function run_test_in_mode(useMozillaPKIX) {
  Services.prefs.setBoolPref("security.use_mozillapkix_verification", useMozillaPKIX);

  // mozilla::pkix doesn't support the obsolete Netscape object signing
  // extension, but NSS does.
  let ee_usage1 = useMozillaPKIX
                ? 'Client,Server,Sign,Encrypt,Object Signer'
                : 'Client,Server,Sign,Encrypt'

  // mozilla::pkix doesn't validate CA certificates for non-CA uses, but
  // NSS does.
  let ca_usage1 = useMozillaPKIX
                ? "SSL CA"
                : 'Client,Server,Sign,Encrypt,SSL CA,Status Responder';

  // Load the ca into mem
  let ca_name = "ca";
  load_cert(ca_name, "CTu,CTu,CTu");
  do_print("ca_name = " + ca_name);
  test_cert_for_usages([ca_name], ca_usage1);

  // A certificate with no basicConstraints extension is considered an EE.
  test_cert_for_usages(["int-no-extensions"], ee_usage1);

  // int-no-extensions is an EE (see previous case), so no certs can chain to
  // it.
  test_cert_for_usages(["ee-int-no-extensions", "int-no-extensions"], "");

  // a certificate with bsaicConstraints.cA==false is considered an EE.
  test_cert_for_usages(["int-not-a-ca"], ee_usage1);

  // int-not-a-ca is an EE (see previous case), so no certs can chain to it.
  test_cert_for_usages(["ee-int-not-a-ca", "int-not-a-ca"], "");

  // int-limited-depth has cA==true and a path length constraint of zero.
  test_cert_for_usages(["int-limited-depth"], ca_usage1);

  // path length constraints do not affect the ability of a non-CA cert to
  // chain to to the CA cert.
  test_cert_for_usages(["ee-int-limited-depth", "int-limited-depth"],
                       ee_usage1);

  // ca
  //   int-limited-depth (cA==true, pathLenConstraint==0)
  //      int-limited-depth-invalid (cA==true)
  //
  // XXX: It seems the NSS code does not consider the path length of the
  // certificate we're validating, but mozilla::pkix does. mozilla::pkix's
  // behavior is correct.
  test_cert_for_usages(["int-limited-depth-invalid", "int-limited-depth"],
                       useMozillaPKIX ? "" : ca_usage1);
  test_cert_for_usages(["ee-int-limited-depth-invalid",
                        "int-limited-depth-invalid",
                        "int-limited-depth"],
                       "");

  // int-valid-ku-no-eku has keyCertSign
  test_cert_for_usages(["int-valid-ku-no-eku"], "SSL CA");
  test_cert_for_usages(["ee-int-valid-ku-no-eku", "int-valid-ku-no-eku"],
                       ee_usage1);

  // int-bad-ku-no-eku has basicConstraints.cA==true and has a KU extension
  // but the KU extension is missing keyCertSign. Note that mozilla::pkix
  // doesn't validate certificates with basicConstraints.Ca==true for non-CA
  // uses, but NSS does.
  test_cert_for_usages(["int-bad-ku-no-eku"],
                       useMozillaPKIX
                          ? ""
                          : 'Client,Server,Sign,Encrypt,Status Responder');
  test_cert_for_usages(["ee-int-bad-ku-no-eku", "int-bad-ku-no-eku"], "");

  // int-no-ku-no-eku has basicConstraints.cA==true and no KU extension.
  // We treat a missing KU as "any key usage is OK".
  test_cert_for_usages(["int-no-ku-no-eku"], ca_usage1);
  test_cert_for_usages(["ee-int-no-ku-no-eku", "int-no-ku-no-eku"], ee_usage1);

  // int-valid-ku-server-eku has basicConstraints.cA==true, keyCertSign in KU,
  // and EKU=={id-kp-serverAuth,id-kp-clientAuth}.
  test_cert_for_usages(["int-valid-ku-server-eku"], "SSL CA");
  test_cert_for_usages(["ee-int-valid-ku-server-eku",
                        "int-valid-ku-server-eku"], "Client,Server");

  // int-bad-ku-server-eku has basicConstraints.cA==true, a KU without
  // keyCertSign, and EKU=={id-kp-serverAuth,id-kp-clientAuth}.
  test_cert_for_usages(["int-bad-ku-server-eku"], "");
  test_cert_for_usages(["ee-int-bad-ku-server-eku", "int-bad-ku-server-eku"],
                       "");

  // int-bad-ku-server-eku has basicConstraints.cA==true, no KU, and
  // EKU=={id-kp-serverAuth,id-kp-clientAuth}.
  test_cert_for_usages(["int-no-ku-server-eku"], "SSL CA");
  test_cert_for_usages(["ee-int-no-ku-server-eku", "int-no-ku-server-eku"],
                       "Client,Server");
}

function run_test() {
  run_test_in_mode(true);
  run_test_in_mode(false);
}
