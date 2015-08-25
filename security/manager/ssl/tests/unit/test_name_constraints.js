// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function certFromFile(name) {
  return constructCertFromFile(`test_name_constraints/${name}.pem`);
}

function load_cert(cert_name, trust_string) {
  addCertFromFile(certdb, `test_name_constraints/${cert_name}.pem`, trust_string);
  return certFromFile(cert_name);
}

function check_cert_err(cert, expected_error) {
  checkCertErrorGeneric(certdb, cert, expected_error, certificateUsageSSLServer);
}

function check_ok(x) {
  return check_cert_err(x, PRErrorCodeSuccess);
}

function check_ok_ca (x) {
  checkCertErrorGeneric(certdb, x, PRErrorCodeSuccess, certificateUsageSSLCA);
}

function check_fail(x) {
  return check_cert_err(x, SEC_ERROR_CERT_NOT_IN_NAME_SPACE);
}

function check_fail_ca(x) {
  checkCertErrorGeneric(certdb, x, SEC_ERROR_CERT_NOT_IN_NAME_SPACE,
                        certificateUsageSSLCA);
}

function run_test() {
  load_cert("ca-nc-perm-foo.com", "CTu,CTu,CTu");
  load_cert("ca-nc", "CTu,CTu,CTu");

  // Note that CN is only looked at when there is NO subjectAltName!

  // Testing with a unconstrained root, and intermediate constrained to PERMIT
  // foo.com. All failures on this section are doe to the cert DNS names
  // not being under foo.com.
  check_ok_ca(load_cert('int-nc-perm-foo.com-ca-nc', ',,'));
  // no dirName
  check_ok(certFromFile('cn-www.foo.com-int-nc-perm-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-int-nc-perm-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-nc-perm-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org-alt-foo.com-int-nc-perm-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com-alt-foo.com-int-nc-perm-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-nc-perm-foo.com-ca-nc'));
  // multiple subjectAltnames
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-nc-perm-foo.com-ca-nc'));
  // C=US O=bar
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-int-nc-perm-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-int-nc-perm-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-nc-perm-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-nc-perm-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-nc-perm-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-nc-perm-foo.com-ca-nc'));
  // multiple subjectAltnames
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-nc-perm-foo.com-ca-nc'));

  // Testing with an unconstrained root and intermediate constrained to
  // EXCLUDE DNS:example.com. All failures on this section are due to the cert
  // DNS names containing example.com. The dirname does not affect evaluation.
  check_ok_ca(load_cert('int-nc-excl-foo.com-ca-nc', ',,'));
  // no dirName
  check_fail(certFromFile('cn-www.foo.com-int-nc-excl-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org-int-nc-excl-foo.com-ca-nc'));
  // notice that since the name constrains apply to the dns name the cn is not
  // evaluated in the case where a subjectAltName exists. Thus the next case is
  // correctly passing.
  check_ok(certFromFile('cn-www.foo.com-alt-foo.org-int-nc-excl-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.com-int-nc-excl-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-int-nc-excl-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org-alt-foo.org-int-nc-excl-foo.com-ca-nc'));
  // multiple subjectAltnames
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-nc-excl-foo.com-ca-nc'));
  // C=US O=bar
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-int-nc-excl-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-int-nc-excl-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-nc-excl-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-nc-excl-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-nc-excl-foo.com-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-nc-excl-foo.com-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-nc-excl-foo.com-ca-nc'));

  // Testing with an unconstrained root, and intermediate constrained to
  // permitting dirName:C=US. All failures on this section are due to cert
  // name not being C=US.
  check_ok_ca(load_cert('int-nc-c-us-ca-nc', ',,'));
  check_fail(certFromFile('cn-www.foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-nc-c-us-ca-nc'));

  // Testing with an unconstrained root, and intermediate constrained to
  // permitting dirNAME:C=US that issues an intermediate name constrained to
  // permitting DNS:foo.com. Checks for inheritance and intersection of
  // different name constraints.
  check_ok_ca(load_cert('int-nc-foo.com-int-nc-c-us-ca-nc', ',,'));
  check_fail(certFromFile('cn-www.foo.com-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.com-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-nc-foo.com-int-nc-c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-nc-foo.com-int-nc-c-us-ca-nc'));

  // Testing on a non constrainted root an intermediate name contrainted to
  // permited dirNAME:C=US and  permited DNS:foo.com
  // checks for compostability of different name constraints with same cert
  check_ok_ca(load_cert('int-nc-perm-foo.com_c-us-ca-nc' , ',,'));
  check_fail(certFromFile('cn-www.foo.com-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.com-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-nc-perm-foo.com_c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-nc-perm-foo.com_c-us-ca-nc'));
  // next check is ok as there is an altname and thus the name constraints do
  // not apply to the common name
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-nc-perm-foo.com_c-us-ca-nc'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-nc-perm-foo.com_c-us-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-nc-perm-foo.com_c-us-ca-nc'));

  // Testing on an unconstrained root and an intermediate name constrained to
  // permitted dirNAME: C=UK all but the intermeduate should fail because they
  // dont have C=UK (missing or C=US)
  check_ok_ca(load_cert('int-nc-perm-c-uk-ca-nc', ',,'));
  check_fail(certFromFile('cn-www.foo.com-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.com-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-nc-perm-c-uk-ca-nc'));

  // Testing on an unconstrained root and an intermediate name constrained to
  // permitted dirNAME: C=UK and an unconstrained intermediate that contains
  // dirNAME C=US. EE and and Intermediates should fail
  check_fail_ca(load_cert('int-c-us-int-nc-perm-c-uk-ca-nc', ',,'));
  check_fail(certFromFile('cn-www.foo.com-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.com-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-c-us-int-nc-perm-c-uk-ca-nc'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-c-us-int-nc-perm-c-uk-ca-nc'));

  // Testing on an unconstrained root and an intermediate name constrained to
  // permitted DNS: foo.com and permitted: DNS: a.us
  check_ok_ca(load_cert('int-nc-foo.com_a.us', ',,'));
  check_ok(certFromFile('cn-www.foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.org-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.org-alt-foo.com-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.com-alt-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-nc-foo.com_a.us'));

  // Testing on an unconstrained root and an intermediate name constrained to
  // permitted DNS: foo.com and permitted: DNS:a.us that issues an intermediate
  // permitted DNS: foo.com .
  // Goal is to ensure that the stricter (inner) name constraint is enforced.
  // The multi-subject alt should fail and is the difference from the sets of
  // tests above.
  check_ok_ca(load_cert('int-nc-foo.com-int-nc-foo.com_a.us', ',,'));
  check_ok(certFromFile('cn-www.foo.com-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.org-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.org-alt-foo.com-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.com-alt-foo.com-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-nc-foo.com-int-nc-foo.com_a.us'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-nc-foo.com-int-nc-foo.com_a.us'));

  // Testing on a root name constrainted to DNS:foo.com and an unconstrained
  // intermediate.
  // Checks that root constraints are enforced.
  check_ok_ca(load_cert('int-ca-nc-perm-foo.com', ',,'));
  check_ok(certFromFile('cn-www.foo.com-int-ca-nc-perm-foo.com'));
  check_fail(certFromFile('cn-www.foo.org-int-ca-nc-perm-foo.com'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.org-int-ca-nc-perm-foo.com'));
  check_ok(certFromFile('cn-www.foo.org-alt-foo.com-int-ca-nc-perm-foo.com'));
  check_ok(certFromFile('cn-www.foo.com-alt-foo.com-int-ca-nc-perm-foo.com'));
  check_fail(certFromFile('cn-www.foo.org-alt-foo.org-int-ca-nc-perm-foo.com'));
  check_fail(certFromFile('cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-int-ca-nc-perm-foo.com'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-int-ca-nc-perm-foo.com'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-int-ca-nc-perm-foo.com'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.org-int-ca-nc-perm-foo.com'));
  check_ok(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.com-int-ca-nc-perm-foo.com'));
  check_ok(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-int-ca-nc-perm-foo.com'));
  check_fail(certFromFile('cn-www.foo.org_o-bar_c-us-alt-foo.org-int-ca-nc-perm-foo.com'));
  check_fail(certFromFile('cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-int-ca-nc-perm-foo.com'));

  // We don't enforce dNSName name constraints on CN unless we're validating
  // for the server EKU. libpkix gets this wrong but mozilla::pkix and classic
  // NSS get it right.
  {
    let cert = certFromFile('cn-www.foo.org-int-nc-perm-foo.com-ca-nc');
    checkCertErrorGeneric(certdb, cert, SEC_ERROR_CERT_NOT_IN_NAME_SPACE,
                          certificateUsageSSLServer);
    checkCertErrorGeneric(certdb, cert, PRErrorCodeSuccess,
                          certificateUsageSSLClient);
  }

  // DCISS tests
  // The certs used here were generated by the NSS test suite and are
  // originally located as security/nss/tests/libpkix/cert/
  load_cert("dciss", "C,C,C");
  check_ok(certFromFile('NameConstraints.dcissallowed'));
  check_fail(certFromFile('NameConstraints.dcissblocked'));
}
