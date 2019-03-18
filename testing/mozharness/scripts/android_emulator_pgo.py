#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import json
import time
import os
import sys
import posixpath
import subprocess

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import BaseScript, PreScriptAction
from mozharness.mozilla.automation import EXIT_STATUS_DICT, TBPL_RETRY
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.testing.android import AndroidMixin
from mozharness.mozilla.testing.testbase import TestingMixin, testing_config_options

PAGES = [
    "js-input/webkit/PerformanceTests/Speedometer/index.html",
    "blueprint/sample.html",
    "blueprint/forms.html",
    "blueprint/grid.html",
    "blueprint/elements.html",
    "js-input/3d-thingy.html",
    "js-input/crypto-otp.html",
    "js-input/sunspider/3d-cube.html",
    "js-input/sunspider/3d-morph.html",
    "js-input/sunspider/3d-raytrace.html",
    "js-input/sunspider/access-binary-trees.html",
    "js-input/sunspider/access-fannkuch.html",
    "js-input/sunspider/access-nbody.html",
    "js-input/sunspider/access-nsieve.html",
    "js-input/sunspider/bitops-3bit-bits-in-byte.html",
    "js-input/sunspider/bitops-bits-in-byte.html",
    "js-input/sunspider/bitops-bitwise-and.html",
    "js-input/sunspider/bitops-nsieve-bits.html",
    "js-input/sunspider/controlflow-recursive.html",
    "js-input/sunspider/crypto-aes.html",
    "js-input/sunspider/crypto-md5.html",
    "js-input/sunspider/crypto-sha1.html",
    "js-input/sunspider/date-format-tofte.html",
    "js-input/sunspider/date-format-xparb.html",
    "js-input/sunspider/math-cordic.html",
    "js-input/sunspider/math-partial-sums.html",
    "js-input/sunspider/math-spectral-norm.html",
    "js-input/sunspider/regexp-dna.html",
    "js-input/sunspider/string-base64.html",
    "js-input/sunspider/string-fasta.html",
    "js-input/sunspider/string-tagcloud.html",
    "js-input/sunspider/string-unpack-code.html",
    "js-input/sunspider/string-validate-input.html",
]


class AndroidProfileRun(TestingMixin, BaseScript, MozbaseMixin,
                        AndroidMixin):
    """
    Mozharness script to generate an android PGO profile using the emulator
    """
    config_options = copy.deepcopy(testing_config_options)

    def __init__(self, require_config_file=False):
        super(AndroidProfileRun, self).__init__(
            config_options=self.config_options,
            all_actions=['setup-avds',
                         'start-emulator',
                         'download',
                         'create-virtualenv',
                         'verify-device',
                         'install',
                         'run-tests',
                         ],
            require_config_file=require_config_file,
            config={
                'virtualenv_modules': [],
                'virtualenv_requirements': [],
                'require_test_zip': True,
                'mozbase_requirements': 'mozbase_source_requirements.txt',
            }
        )

        # these are necessary since self.config is read only
        c = self.config
        self.installer_path = c.get('installer_path')
        self.device_serial = 'emulator-5554'

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(AndroidProfileRun, self).query_abs_dirs()
        dirs = {}

        dirs['abs_test_install_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'src', 'testing')
        dirs['abs_xre_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'hostutils')
        dirs['abs_blob_upload_dir'] = '/builds/worker/artifacts/blobber_upload_dir'
        dirs['abs_avds_dir'] = self.config.get("avds_dir", "/home/cltbld/.android")

        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    ##########################################
    # Actions for AndroidProfileRun        #
    ##########################################

    def preflight_install(self):
        # in the base class, this checks for mozinstall, but we don't use it
        pass

    @PreScriptAction('create-virtualenv')
    def pre_create_virtualenv(self, action):
        dirs = self.query_abs_dirs()
        self.register_virtualenv_module(
            'marionette',
            os.path.join(dirs['abs_work_dir'], 'src', 'testing', 'marionette', 'client')
        )

    def download(self):
        """
        Download host utilities
        """
        dirs = self.query_abs_dirs()
        self.xre_path = self.download_hostutils(dirs['abs_xre_dir'])

    def install(self):
        """
        Install APKs on the device.
        """
        assert self.installer_path is not None, \
            "Either add installer_path to the config or use --installer-path."
        self.install_apk(self.installer_path)
        self.info("Finished installing apps for %s" % self.device_serial)

    def run_tests(self):
        """
        Generate the PGO profile data
        """
        from mozhttpd import MozHttpd
        from mozprofile import Preferences
        from mozdevice import ADBDevice, ADBProcessError, ADBTimeoutError
        from six import string_types
        from marionette_driver.marionette import Marionette

        app = self.query_package_name()

        IP = '10.0.2.2'
        PORT = 8888

        PATH_MAPPINGS = {
            '/js-input/webkit/PerformanceTests': 'third_party/webkit/PerformanceTests',
        }

        dirs = self.query_abs_dirs()
        topsrcdir = os.path.join(dirs['abs_work_dir'], 'src')
        adb = self.query_exe('adb')

        path_mappings = {
            k: os.path.join(topsrcdir, v)
            for k, v in PATH_MAPPINGS.items()
        }
        httpd = MozHttpd(
            port=PORT,
            docroot=os.path.join(topsrcdir, "build", "pgo"),
            path_mappings=path_mappings)
        httpd.start(block=False)

        profile_data_dir = os.path.join(topsrcdir, 'testing', 'profiles')
        with open(os.path.join(profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['profileserver']

        prefpaths = [
            os.path.join(profile_data_dir, profile, 'user.js')
            for profile in base_profiles
        ]

        prefs = {}
        for path in prefpaths:
            prefs.update(Preferences.read_prefs(path))

        interpolation = {"server": "%s:%d" % httpd.httpd.server_address,
                         "OOP": "false"}
        for k, v in prefs.items():
            if isinstance(v, string_types):
                v = v.format(**interpolation)
            prefs[k] = Preferences.cast(v)

        outputdir = self.config.get('output_directory', '/sdcard')
        jarlog = posixpath.join(outputdir, 'en-US.log')
        profdata = posixpath.join(outputdir, 'default.profraw')

        env = {}
        env["XPCOM_DEBUG_BREAK"] = "warn"
        env["MOZ_IN_AUTOMATION"] = "1"
        env["MOZ_JAR_LOG_FILE"] = jarlog
        env["LLVM_PROFILE_FILE"] = profdata

        adbdevice = ADBDevice(adb=adb,
                              device='emulator-5554')

        try:
            # Run Fennec a first time to initialize its profile
            driver = Marionette(
                app='fennec',
                package_name=app,
                adb_path=adb,
                bin="target.apk",
                prefs=prefs,
                connect_to_running_emulator=True,
                startup_timeout=1000,
                env=env,
            )
            driver.start_session()

            # Now generate the profile and wait for it to complete
            for page in PAGES:
                driver.navigate("http://%s:%d/%s" % (IP, PORT, page))
                timeout = 2
                if 'Speedometer/index.html' in page:
                    # The Speedometer test actually runs many tests internally in
                    # javascript, so it needs extra time to run through them. The
                    # emulator doesn't get very far through the whole suite, but
                    # this extra time at least lets some of them process.
                    timeout = 360
                time.sleep(timeout)

            driver.set_context("chrome")
            driver.execute_script("""
                Components.utils.import("resource://gre/modules/Services.jsm");
                let cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
                    .createInstance(Components.interfaces.nsISupportsPRBool);
                Services.obs.notifyObservers(cancelQuit, "quit-application-requested", null);
                return cancelQuit.data;
            """)
            driver.execute_script("""
                Components.utils.import("resource://gre/modules/Services.jsm");
                Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit)
            """)

            # There is a delay between execute_script() returning and the profile data
            # actually getting written out, so poll the device until we get a profile.
            for i in range(50):
                try:
                    localprof = '/builds/worker/workspace/default.profraw'
                    adbdevice.pull(profdata, localprof)
                    stats = os.stat(localprof)
                    if stats.st_size == 0:
                        # The file may not have been fully written yet, so retry until we
                        # get actual data.
                        time.sleep(2)
                    else:
                        break
                except ADBProcessError:
                    # The file may not exist at all yet, which would raise an
                    # ADBProcessError, so retry.
                    time.sleep(2)
            else:
                raise Exception("Unable to pull default.profraw")
            adbdevice.pull(jarlog, '/builds/worker/workspace/en-US.log')
        except ADBTimeoutError:
            self.fatal('INFRA-ERROR: Failed with an ADBTimeoutError',
                       EXIT_STATUS_DICT[TBPL_RETRY])

        # tarfile doesn't support xz in this version of Python
        tar_cmd = [
            'tar',
            '-acvf',
            '/builds/worker/artifacts/profdata.tar.xz',
            '-C', '/builds/worker/workspace',
            'default.profraw',
            'en-US.log',
        ]
        subprocess.check_call(tar_cmd)

        httpd.stop()


if __name__ == '__main__':
    test = AndroidProfileRun()
    test.run_and_exit()
