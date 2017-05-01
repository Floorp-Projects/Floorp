'''This helps loading mitmproxy's cert and change proxy settings for Firefox.'''
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from mozharness.mozilla.firefox.autoconfig import write_autoconfig_files

DEFAULT_CERT_PATH = os.path.join(os.getenv('HOME'),
                                 '.mitmproxy', 'mitmproxy-ca-cert.cer')
MITMPROXY_SETTINGS = '''// Start with a comment
// Load up mitmproxy cert
var Cc = Components.classes;
var Ci = Components.interfaces;
var certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);
var certdb2 = certdb;

try {
certdb2 = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB2);
} catch (e) {}

cert = "%(cert)s";
certdb2.addCertFromBase64(cert, "C,C,C", "");

// Use mitmdump as the proxy
// Manual proxy configuration
pref("network.proxy.type", 1);
pref("network.proxy.http", "127.0.0.1");
pref("network.proxy.http_port", 8080);
pref("network.proxy.ssl", "127.0.0.1");
pref("network.proxy.ssl_port", 8080);
'''


def configure_mitmproxy(fx_install_dir, certificate_path=DEFAULT_CERT_PATH):
        certificate = _read_certificate(certificate_path)
        write_autoconfig_files(fx_install_dir=fx_install_dir,
                               cfg_contents=MITMPROXY_SETTINGS % {
                                  'cert': certificate})


def _read_certificate(certificate_path):
    ''' Return the certificate's hash from the certificate file.'''
    # NOTE: mitmproxy's certificates do not exist until one of its binaries
    #       has been executed once on the host
    with open(certificate_path, 'r') as fd:
        contents = fd.read()
    return ''.join(contents.splitlines()[1:-1])
