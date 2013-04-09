"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;
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

let tempScope = {};
Cu.import("resource://gre/modules/NetUtil.jsm", tempScope);
let NetUtil = tempScope.NetUtil;


Cu.import("resource://gre/modules/FileUtils.jsm"); // XXX: tempScope?
Cu.import("resource://gre/modules/Services.jsm");  // XXX: tempScope?

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

var ca_usages = ['Client,Server,Sign,Encrypt,SSL CA,Status Responder',
                 'SSL CA',
                 'Client,Server,Sign,Encrypt,SSL CA,Status Responder',

/* XXX
  When using PKIX the usage for the fourth CA (key usage= all but certsigning) the
  usage returned is 'Status Responder' only. (no SSL CA)
  Thus (self-consistently) the ee_usages are all empty.

  Reading rfc 5280 the combination of basic contraints+usages is not clear:
   - Asserted key usage keyCertSign MUST have CA basic contraints.
   - CA NOT asserted => keyCertSignt MUST NOT be asserted.

  But the tests include -> basic CA asserted and Keyusage NOT asserted.
  In this case, the 'classic' mode uses the 'union' of the capabilites ->
  the cert is considered a CA. libpkix uses the 'intersection' of
  capabilites, the cert is NOT considered a CA.

  I think the intersection is a better way to interpret this as the extension
  is marked as critical.

  This should be: 'Status Responder'

*/
                 'Client,Server,Sign,Encrypt,Status Responder'];

var ee_usages = [
                  ['Client,Server,Sign,Encrypt',
                   'Client,Server,Sign,Encrypt',
                   'Client,Server,Sign,Encrypt',
                   '',
                   'Client,Server,Sign,Encrypt,Object Signer,Status Responder',
                   'Client,Server',
                   'Sign,Encrypt,Object Signer',
                   'Server,Status Responder'
                   ],
                  ['Client,Server,Sign,Encrypt',
                   'Client,Server,Sign,Encrypt',
                   'Client,Server,Sign,Encrypt',
                   '',
                   'Client,Server,Sign,Encrypt,Object Signer,Status Responder',
                   'Client,Server',
                   'Sign,Encrypt,Object Signer',
                   'Server,Status Responder'
                   ],
                  ['Client,Server,Sign,Encrypt',
                   'Client,Server,Sign,Encrypt',
                   'Client,Server,Sign,Encrypt',
                   '',
                   'Client,Server,Sign,Encrypt,Object Signer,Status Responder',
                   'Client,Server',
                   'Sign,Encrypt,Object Signer',
                   'Server,Status Responder'
                  ],

/* XXX
  The values for the following block of tests should all be empty
  */

                  ['Client,Server,Sign,Encrypt',
                   'Client,Server,Sign,Encrypt',
                   'Client,Server,Sign,Encrypt',
                   '',
                   'Client,Server,Sign,Encrypt,Object Signer,Status Responder',
                   'Client,Server',
                   'Sign,Encrypt,Object Signer',
                   'Server,Status Responder'
                  ]
                ];


function run_test() {
  //ca's are one based!
  for (var i = 0; i < ca_usages.length; i++) {
    var ca_name = "ca-" + (i + 1);
    var ca_filename = ca_name + ".der";
    var root_cert_der =
      do_get_file("test_certificate_usages/" + ca_filename, false);
    var der = readFile(root_cert_der);
    certdb.addCert(der, "CTu,CTu,CTu", ca_name);

    do_print("ca_name=" + ca_name);
    var cert;
    cert = certdb.findCertByNickname(null, ca_name);

    var verified = {};
    var usages = {};
    cert.getUsagesString(true, verified, usages);
    do_print("usages.value=" + usages.value);
    do_check_eq(ca_usages[i], usages.value);

    //now the ee, names also one based
    for (var j = 0; j < ee_usages[i].length; j++) {
      var ee_name = "ee-" + (j + 1) + "-" + ca_name;
      var ee_filename = ee_name + ".der";
      //do_print("ee_filename" + ee_filename);
      var ee_cert_der =
        do_get_file("test_certificate_usages/" + ee_filename, false);
      var der = readFile(ee_cert_der);
      certdb.addCert(der, ",,", ee_name);
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
