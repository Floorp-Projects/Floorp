'''This helps loading mitmproxy's cert and change proxy settings for Firefox.'''
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os
import signal
import subprocess
import sys

import time

import mozinfo

from mozlog import get_proxy_logger
from mozprocess import ProcessHandler

from .base import Playback

here = os.path.dirname(os.path.realpath(__file__))
LOG = get_proxy_logger(component='mitmproxy')

mozharness_dir = os.path.join(here, '../../../mozharness')
sys.path.insert(0, mozharness_dir)

external_tools_path = os.environ.get('EXTERNALTOOLSPATH', None)

if external_tools_path is not None:
    # running in production via mozharness
    TOOLTOOL_PATH = os.path.join(external_tools_path, 'tooltool.py')
else:
    # running locally via mach
    TOOLTOOL_PATH = os.path.join(mozharness_dir, 'external_tools', 'tooltool.py')

# path for mitmproxy certificate, generated auto after mitmdump is started
# on local machine it is 'HOME', however it is different on production machines
try:
    DEFAULT_CERT_PATH = os.path.join(os.getenv('HOME'),
                                     '.mitmproxy', 'mitmproxy-ca-cert.cer')
except Exception:
    DEFAULT_CERT_PATH = os.path.join(os.getenv('HOMEDRIVE'), os.getenv('HOMEPATH'),
                                     '.mitmproxy', 'mitmproxy-ca-cert.cer')

MITMPROXY_SETTINGS = '''// Start with a comment
// Load up mitmproxy cert
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


class Mitmproxy(Playback):

    def __init__(self, config):
        self.config = config
        self.mitmproxy_proc = None
        self.recordings = config.get('playback_recordings', None)
        self.browser_path = config.get('binary', None)

        # bindir is where we will download all mitmproxy required files
        # if invoved via mach we will have received this in config; otherwise
        # not running via mach (invoved direcdtly in testing/raptor) so figure it out
        if self.config.get("obj_path", None) is not None:
            self.bindir = self.config.get("obj_path")
        else:
            # bit of a pain to get object dir when not running via mach - need to go from
            # the binary folder i.e.
            # /mozilla-unified/obj-x86_64-apple-darwin17.4.0/dist/Nightly.app/Contents/MacOS/
            # back to:
            # mozilla-unified/obj-x86_64-apple-darwin17.4.0/
            # note, this may need to be updated per platform
            self.bindir = os.path.normpath(os.path.join(self.config['binary'],
                                                        '..', '..', '..', '..',
                                                        '..', 'testing', 'raptor'))

        self.recordings_path = self.bindir
        LOG.info("bindir to be used for mitmproxy downloads and exe files: %s" % self.bindir)

        # go ahead and download and setup mitmproxy
        self.download()
        # mitmproxy must be started before setup, so that the CA cert is available
        self.start()
        self.setup()

    def _tooltool_fetch(self, manifest):
        def outputHandler(line):
            LOG.info(line)
        command = [sys.executable, TOOLTOOL_PATH, 'fetch', '-o', '-m', manifest]

        proc = ProcessHandler(
            command, processOutputLine=outputHandler, storeOutput=False,
            cwd=self.bindir)

        proc.run()

        try:
            proc.wait()
        except Exception:
            if proc.poll() is None:
                proc.kill(signal.SIGTERM)

    def download(self):
        # download mitmproxy binary and pageset using tooltool
        # note: tooltool automatically unpacks the files as well
        if not os.path.exists(self.bindir):
            os.makedirs(self.bindir)
        LOG.info("downloading mitmproxy binary")
        _manifest = os.path.join(here, self.config['playback_binary_manifest'])
        self._tooltool_fetch(_manifest)
        LOG.info("downloading mitmproxy pageset")
        _manifest = os.path.join(here, self.config['playback_pageset_manifest'])
        self._tooltool_fetch(_manifest)
        return

    def setup(self):
        # install the generated CA certificate into Firefox
        # mitmproxy cert setup needs path to mozharness install; mozharness has set this
        # value in the SCRIPTSPATH env var for us in mozharness/mozilla/testing/talos.py
        scripts_path = os.environ.get('SCRIPTSPATH')
        LOG.info('scripts_path: %s' % str(scripts_path))
        self.install_mitmproxy_cert(self.mitmproxy_proc,
                                    self.browser_path,
                                    str(scripts_path))
        return

    def start(self):
        mitmdump_path = os.path.join(self.bindir, 'mitmdump')
        recordings_list = self.recordings.split()
        self.mitmproxy_proc = self.start_mitmproxy_playback(mitmdump_path,
                                                            self.recordings_path,
                                                            recordings_list,
                                                            self.browser_path)
        return

    def stop(self):
        self.stop_mitmproxy_playback()
        return

    def configure_mitmproxy(self,
                            fx_install_dir,
                            scripts_path,
                            certificate_path=DEFAULT_CERT_PATH):
        # scripts_path is path to mozharness on test machine; needed so can import
        if scripts_path is not False:
            sys.path.insert(1, scripts_path)
            sys.path.insert(1, os.path.join(scripts_path, 'mozharness'))
        from mozharness.mozilla.firefox.autoconfig import write_autoconfig_files
        certificate = self._read_certificate(certificate_path)
        write_autoconfig_files(fx_install_dir=fx_install_dir,
                               cfg_contents=MITMPROXY_SETTINGS % {
                                  'cert': certificate})

    def _read_certificate(self, certificate_path):
        ''' Return the certificate's hash from the certificate file.'''
        # NOTE: mitmproxy's certificates do not exist until one of its binaries
        #       has been executed once on the host
        with open(certificate_path, 'r') as fd:
            contents = fd.read()
        return ''.join(contents.splitlines()[1:-1])

    def is_mitmproxy_cert_installed(self, browser_install):
        """Verify mitmxproy CA cert was added to Firefox"""
        from mozharness.mozilla.firefox.autoconfig import read_autoconfig_file
        try:
            # read autoconfig file, confirm mitmproxy cert is in there
            certificate = self._read_certificate(DEFAULT_CERT_PATH)
            contents = read_autoconfig_file(browser_install)
            if (MITMPROXY_SETTINGS % {'cert': certificate}) in contents:
                LOG.info("Verified mitmproxy CA certificate is installed in Firefox")
            else:
                LOG.info("Firefox autoconfig file contents:")
                LOG.info(contents)
                return False
        except Exception:
            LOG.info("Failed to read Firefox autoconfig file, when verifying CA cert install")
            return False
        return True

    def install_mitmproxy_cert(self, mitmproxy_proc, browser_path, scripts_path):
        """Install the CA certificate generated by mitmproxy, into Firefox"""
        LOG.info("Installing mitmxproxy CA certficate into Firefox")
        # browser_path is exe, we want install dir
        browser_install = os.path.dirname(browser_path)
        # on macosx we need to remove the last folders 'Content/MacOS'
        if mozinfo.os == 'mac':
            browser_install = browser_install[:-14]

        LOG.info('Calling configure_mitmproxy with browser folder: %s' % browser_install)
        self.configure_mitmproxy(browser_install, scripts_path)
        # cannot continue if failed to add CA cert to Firefox, need to check
        if not self.is_mitmproxy_cert_installed(browser_install):
            LOG.error('Aborting: failed to install mitmproxy CA cert into Firefox')
            self.stop_mitmproxy_playback(mitmproxy_proc)
            sys.exit()

    def start_mitmproxy_playback(self,
                                 mitmdump_path,
                                 mitmproxy_recording_path,
                                 mitmproxy_recordings_list,
                                 browser_path):
        """Startup mitmproxy and replay the specified flow file"""

        LOG.info("mitmdump path: %s" % mitmdump_path)
        LOG.info("recording path: %s" % mitmproxy_recording_path)
        LOG.info("recordings list: %s" % mitmproxy_recordings_list)
        LOG.info("browser path: %s" % browser_path)

        mitmproxy_recordings = []
        # recording names can be provided in comma-separated list; build py list including path
        for recording in mitmproxy_recordings_list:
            mitmproxy_recordings.append(os.path.join(mitmproxy_recording_path, recording))

        # cmd line to start mitmproxy playback using custom playback script is as follows:
        # <path>/mitmdump -s "<path>mitmdump-alternate-server-replay/alternate-server-replay.py
        #  <path>recording-1.mp <path>recording-2.mp..."
        param = os.path.join(here, 'alternate-server-replay.py')
        env = os.environ.copy()

        # this part is platform-specific
        if mozinfo.os == 'win':
            param2 = '""' + param.replace('\\', '\\\\\\') + ' ' + \
                     ' '.join(mitmproxy_recordings).replace('\\', '\\\\\\') + '""'
            sys.path.insert(1, mitmdump_path)
        else:
            # mac and linux
            param2 = param + ' ' + ' '.join(mitmproxy_recordings)

        # mitmproxy needs some DLL's that are a part of Firefox itself, so add to path
        env["PATH"] = os.path.dirname(browser_path) + ";" + env["PATH"]

        command = [mitmdump_path, '-k', '-q', '-s', param2]

        LOG.info("Starting mitmproxy playback using env path: %s" % env["PATH"])
        LOG.info("Starting mitmproxy playback using command: %s" % ' '.join(command))
        # to turn off mitmproxy log output, use these params for Popen:
        # Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        mitmproxy_proc = subprocess.Popen(command, env=env)
        time.sleep(10)
        data = mitmproxy_proc.poll()
        if data is None:  # None value indicates process hasn't terminated
            LOG.info("Mitmproxy playback successfully started as pid %d" % mitmproxy_proc.pid)
            return mitmproxy_proc
        # cannot continue as we won't be able to playback the pages
        LOG.error('Aborting: mitmproxy playback process failed to start, poll returned: %s' % data)
        sys.exit()

    def stop_mitmproxy_playback(self):
        """Stop the mitproxy server playback"""
        mitmproxy_proc = self.mitmproxy_proc
        LOG.info("Stopping mitmproxy playback, klling process %d" % mitmproxy_proc.pid)
        if mozinfo.os == 'win':
            mitmproxy_proc.kill()
        else:
            mitmproxy_proc.terminate()
        time.sleep(10)
        status = mitmproxy_proc.poll()
        if status is None:  # None value indicates process hasn't terminated
            # I *think* we can still continue, as process will be automatically
            # killed anyway when mozharness is done (?) if not, we won't be able
            # to startup mitmxproy next time if it is already running
            LOG.error("Failed to kill the mitmproxy playback process")
            LOG.info(str(status))
        else:
            LOG.info("Successfully killed the mitmproxy playback process")
