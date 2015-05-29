"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function load_cert(name, trust) {
  let filename = "test_intermediate_basic_usage_constraints/" + name + ".pem";
  addCertFromFile(certdb, filename, trust);
}

function test_cert_for_usages(certChainNicks, expected_usages_string) {
  let certs = [];
  for (let i in certChainNicks) {
    let certNick = certChainNicks[i];
    let certPEM = readFile(
                    do_get_file("test_intermediate_basic_usage_constraints/"
                                + certNick + ".pem"), false);
    certs.push(certdb.constructX509FromBase64(pemToBase64(certPEM)));
  }

  let cert = certs[0];
  let verified = {};
  let usages = {};
  cert.getUsagesString(true, verified, usages);
  do_print("usages.value = " + usages.value);
  do_check_eq(expected_usages_string, usages.value);
}

function run_test() {
  let ee_usage1 = 'Client,Server,Sign,Encrypt,Object Signer';
  let ca_usage1 = "SSL CA";

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

  // a certificate with basicConstraints.cA==false is considered an EE.
  test_cert_for_usages(["int-not-a-ca"], ee_usage1);

  // int-not-a-ca is an EE (see previous case), so no certs can chain to it.
  test_cert_for_usages(["ee-int-not-a-ca", "int-not-a-ca"], "");

  // a certificate with basicConstraints.cA==false but with the keyCertSign
  // key usage may not act as a CA (it can act like an end-entity).
  test_cert_for_usages(["int-cA-FALSE-asserts-keyCertSign"], ee_usage1);
  test_cert_for_usages(["ee-int-cA-FALSE-asserts-keyCertSign",
                        "int-cA-FALSE-asserts-keyCertSign"], "");


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
  test_cert_for_usages(["int-limited-depth-invalid", "int-limited-depth"], "");
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
  // uses.
  test_cert_for_usages(["int-bad-ku-no-eku"], "");
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
