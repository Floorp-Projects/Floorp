'''Functions to download, install, setup, and use the mitmproxy playback tool'''
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
LOG = get_proxy_logger(component='raptor-mitmproxy')

# needed so unit tests can find their imports
if os.environ.get('SCRIPTSPATH', None) is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ['SCRIPTSPATH']
else:
    # locally it's in source tree
    mozharness_dir = os.path.join(here, '../../../mozharness')
sys.path.insert(0, mozharness_dir)

# required for using a python3 virtualenv on win for mitmproxy
from mozharness.base.python import Python3Virtualenv
from mozharness.mozilla.testing.testbase import TestingMixin
from mozharness.base.vcs.vcsbase import MercurialScript

raptor_dir = os.path.join(here, '..')
sys.path.insert(0, raptor_dir)

from utils import transform_platform

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


class Mitmproxy(Playback, Python3Virtualenv, TestingMixin, MercurialScript):

    def __init__(self, config):
        self.config = config
        self.mitmproxy_proc = None
        self.mitmdump_path = None
        self.recordings = config.get('playback_recordings', None)
        self.browser_path = config.get('binary', None)

        # raptor_dir is where we will download all mitmproxy required files
        # when running locally it comes from obj_path via mozharness/mach
        if self.config.get("obj_path", None) is not None:
            self.raptor_dir = self.config.get("obj_path")
        else:
            # in production it is ../tasks/task_N/build/, in production that dir
            # is not available as an envvar, however MOZ_UPLOAD_DIR is set as
            # ../tasks/task_N/build/blobber_upload_dir so take that and go up 1 level
            self.raptor_dir = os.path.dirname(os.path.dirname(os.environ['MOZ_UPLOAD_DIR']))

        # add raptor to raptor_dir
        self.raptor_dir = os.path.join(self.raptor_dir, "testing", "raptor")
        self.recordings_path = self.raptor_dir
        LOG.info("raptor_dir used for mitmproxy downloads and exe files: %s" % self.raptor_dir)

        # go ahead and download and setup mitmproxy
        self.download()

        # on windows we must use a python3 virtualen for mitmproxy
        if 'win' in self.config['platform']:
            self.setup_py3_virtualenv()

        # mitmproxy must be started before setup, so that the CA cert is available
        self.start()
        self.setup()

    def _tooltool_fetch(self, manifest):
        def outputHandler(line):
            LOG.info(line)
        command = [sys.executable, TOOLTOOL_PATH, 'fetch', '-o', '-m', manifest]

        proc = ProcessHandler(
            command, processOutputLine=outputHandler, storeOutput=False,
            cwd=self.raptor_dir)

        proc.run()

        try:
            proc.wait()
        except Exception:
            if proc.poll() is None:
                proc.kill(signal.SIGTERM)

    def download(self):
        # download mitmproxy binary and pageset using tooltool
        # note: tooltool automatically unpacks the files as well
        if not os.path.exists(self.raptor_dir):
            os.makedirs(self.raptor_dir)

        if 'win' in self.config['platform']:
            # on windows we need a python3 environment and use our own package from tooltool
            self.py3_path = self.fetch_python3()
            LOG.info("python3 path is: %s" % self.py3_path)
        else:
            # on osx and linux we use pre-built binaries
            LOG.info("downloading mitmproxy binary")
            _manifest = os.path.join(here, self.config['playback_binary_manifest'])
            transformed_manifest = transform_platform(_manifest, self.config['platform'])
            self._tooltool_fetch(transformed_manifest)

        # we use one pageset for all platforms (pageset was recorded on win10)
        LOG.info("downloading mitmproxy pageset")
        _manifest = os.path.join(here, self.config['playback_pageset_manifest'])
        transformed_manifest = transform_platform(_manifest, self.config['platform'])
        self._tooltool_fetch(transformed_manifest)
        return

    def fetch_python3(self):
        """Mitmproxy on windows needs Python 3.x"""
        python3_path = os.path.join(self.raptor_dir, 'python3.6', 'python')
        if not os.path.exists(os.path.dirname(python3_path)):
            _manifest = os.path.join(here, self.config['python3_win_manifest'])
            transformed_manifest = transform_platform(_manifest, self.config['platform'],
                                                      self.config['processor'])
            LOG.info("downloading py3 package for mitmproxy windows: %s" % transformed_manifest)
            self._tooltool_fetch(transformed_manifest)
        cmd = [python3_path, '--version']
        # just want python3 ver printed in production log
        subprocess.Popen(cmd, env=os.environ.copy())
        return python3_path

    def setup_py3_virtualenv(self):
        """Mitmproxy on windows needs Python 3.x; set up a separate py 3.x env here"""
        LOG.info("Setting up python 3.x virtualenv, required for mitmproxy on windows")
        # these next two are required for py3_venv_configuration
        self.abs_dirs = {'base_work_dir': mozharness_dir}
        self.log_obj = None
        # now create the py3 venv
        venv_path = os.path.join(self.raptor_dir, 'py3venv')
        self.py3_venv_configuration(python_path=self.py3_path, venv_path=venv_path)
        self.py3_create_venv()
        self.py3_install_modules(["cffi==1.10.0"])
        requirements = [os.path.join(here, "mitmproxy_requirements.txt")]
        self.py3_install_requirement_files(requirements)
        # add py3 executables path to system path
        sys.path.insert(1, self.py3_path_to_executables())
        # install mitmproxy itself
        self.py3_install_modules(modules=['mitmproxy'])
        self.mitmdump_path = os.path.join(self.py3_path_to_executables(), 'mitmdump')

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
        # if on windows, the mitmdump_path was already set when creating py3 env
        if self.mitmdump_path is None:
            self.mitmdump_path = os.path.join(self.raptor_dir, 'mitmdump')

        recordings_list = self.recordings.split()
        self.mitmproxy_proc = self.start_mitmproxy_playback(self.mitmdump_path,
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
