"use strict";

/* To regenerate the certificates and apps for this test:

        cd security/manager/ssl/tests/unit/test_certificate_usages
        PATH=$NSS/bin:$NSS/lib:$PATH ./generate.pl
        cd ../../../../../..
        make -C $OBJDIR/security/manager/ssl/tests

   $NSS is the path to NSS binaries and libraries built for the host platform.
   If you get error messages about "CertUtil" on Windows, then it means that
   the Windows CertUtil.exe is ahead of the NSS certutil.exe in $PATH.

   Check in the generated files. These steps are not done as part of the build
   because we do not want to add a build-time dependency on the OpenSSL or NSS
   tools or libraries built for the host platform.
*/

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);

const gNumCAs = 4;

function run_test() {
  //ca's are one based!
  for (var i = 0; i < gNumCAs; i++) {
    var ca_name = "ca-" + (i + 1);
    var ca_filename = ca_name + ".der";
    addCertFromFile(certdb, "test_certificate_usages/" + ca_filename, "CTu,CTu,CTu");
    do_print("ca_name=" + ca_name);
    var cert = certdb.findCertByNickname(null, ca_name);
  }

  run_test_in_mode(true);
  run_test_in_mode(false);
}

function run_test_in_mode(useMozillaPKIX) {
  Services.prefs.setBoolPref("security.use_mozillapkix_verification", useMozillaPKIX);
  clearOCSPCache();
  clearSessionCache();

  // mozilla::pkix does not allow CA certs to be validated for non-CA usages.
  var allCAUsages = useMozillaPKIX
                  ? 'SSL CA'
                  : 'Client,Server,Sign,Encrypt,SSL CA,Status Responder';

  // mozilla::pkix doesn't allow CA certificates to have the Status Responder
  // EKU.
  var ca_usages = [allCAUsages,
                   'SSL CA',
                   allCAUsages,
                   useMozillaPKIX ? ''
                                  : 'Client,Server,Sign,Encrypt,Status Responder'];

  // mozilla::pkix doesn't implement the Netscape Object Signer restriction.
  var basicEndEntityUsages = useMozillaPKIX
                           ? 'Client,Server,Sign,Encrypt,Object Signer'
                           : 'Client,Server,Sign,Encrypt';
  var basicEndEntityUsagesWithObjectSigner = basicEndEntityUsages + ",Object Signer"

  // mozilla::pkix won't let a certificate with the "Status Responder" EKU get
  // validated for any other usage.
  var statusResponderUsages = (useMozillaPKIX ? "" : "Server,") + "Status Responder";
  var statusResponderUsagesFull
      = useMozillaPKIX ? statusResponderUsages
                       : basicEndEntityUsages + ',Object Signer,Status Responder';

  var ee_usages = [
    [ basicEndEntityUsages,
      basicEndEntityUsages,
      basicEndEntityUsages,
      '',
      statusResponderUsagesFull,
      'Client,Server',
      'Sign,Encrypt,Object Signer',
      statusResponderUsages
    ],

    [ basicEndEntityUsages,
      basicEndEntityUsages,
      basicEndEntityUsages,
      '',
      statusResponderUsagesFull,
      'Client,Server',
      'Sign,Encrypt,Object Signer',
      statusResponderUsages
    ],

    [ basicEndEntityUsages,
      basicEndEntityUsages,
      basicEndEntityUsages,
      '',
      statusResponderUsagesFull,
      'Client,Server',
      'Sign,Encrypt,Object Signer',
      statusResponderUsages
    ],

    // The CA has isCA=true without keyCertSign.
    //
    // The 'classic' NSS mode uses the 'union' of the
    // capabilites so the cert is considered a CA.
    // mozilla::pkix and libpkix use the intersection of
    // capabilites, so the cert is NOT considered a CA.
    [ useMozillaPKIX ? '' : basicEndEntityUsages,
      useMozillaPKIX ? '' : basicEndEntityUsages,
      useMozillaPKIX ? '' : basicEndEntityUsages,
      '',
      useMozillaPKIX ? '' : statusResponderUsagesFull,
      useMozillaPKIX ? '' : 'Client,Server',
      useMozillaPKIX ? '' : 'Sign,Encrypt,Object Signer',
      useMozillaPKIX ? '' : 'Server,Status Responder'
     ]
  ];

  do_check_eq(gNumCAs, ca_usages.length);

  for (var i = 0; i < gNumCAs; i++) {
    var ca_name = "ca-" + (i + 1);
    var verified = {};
    var usages = {};
    var cert = certdb.findCertByNickname(null, ca_name);
    cert.getUsagesString(true, verified, usages);
    do_print("usages.value=" + usages.value);
    do_check_eq(ca_usages[i], usages.value);
    if (ca_usages[i].indexOf('SSL CA') != -1) {
      checkCertErrorGeneric(certdb, cert, 0, certificateUsageVerifyCA);
    }
    //now the ee, names also one based
    for (var j = 0; j < ee_usages[i].length; j++) {
      var ee_name = "ee-" + (j + 1) + "-" + ca_name;
      var ee_filename = ee_name + ".der";
      //do_print("ee_filename" + ee_filename);
      addCertFromFile(certdb, "test_certificate_usages/" + ee_filename, ",,");
      var ee_cert;
      ee_cert = certdb.findCertByNickname(null, ee_name);
      var verified = {};
      var usages = {};
      ee_cert.getUsagesString(true, verified, usages);
      do_print("cert usages.value=" + usages.value);
      do_check_eq(ee_usages[i][j], usages.value);
    }
  }
}
