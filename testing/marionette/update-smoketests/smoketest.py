# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import subprocess
import sys
import tempfile
import threading
import zipfile

from ConfigParser import ConfigParser

this_dir = os.path.abspath(os.path.dirname(__file__))
marionette_dir = os.path.dirname(this_dir)
marionette_client_dir = os.path.join(marionette_dir, 'client', 'marionette')

def find_b2g():
    sys.path.append(marionette_client_dir)
    from b2gbuild import B2GBuild
    return B2GBuild()

class DictObject(dict):
    def __getattr__(self, item):
        try:
            return self.__getitem__(item)
        except KeyError:
            raise AttributeError(item)

    def __getitem__(self, item):
        value = dict.__getitem__(self, item)
        if isinstance(value, dict):
            return DictObject(value)
        return value

class SmokeTestError(Exception):
    pass

class SmokeTestConfigError(SmokeTestError):
    def __init__(self, message):
        SmokeTestError.__init__(self, 'smoketest-config.json: ' + message)

class SmokeTestConfig(DictObject):
    TOP_LEVEL_REQUIRED = ('devices', 'public_key', 'private_key')
    DEVICE_REQUIRED    = ('system_fs_type', 'system_location', 'data_fs_type',
                          'data_location', 'sdcard', 'sdcard_recovery',
                          'serials')

    def __init__(self, build_dir):
        self.top_dir = build_dir
        self.build_data = {}
        self.flash_template = None

        with open(os.path.join(build_dir, 'smoketest-config.json')) as f:
            DictObject.__init__(self, json.loads(f.read()))

        for required in self.TOP_LEVEL_REQUIRED:
            if required not in self:
                raise SmokeTestConfigError('No "%s" found' % required)

        if len(self.devices) == 0:
            raise SmokeTestConfigError('No devices found')

        for name, device in self.devices.iteritems():
            for required in self.DEVICE_REQUIRED:
                if required not in device:
                    raise SmokeTestConfigError('No "%s" found in device "%s"' % (required, name))

    def get_build_data(self, device, build_id):
        if device in self.build_data:
            if build_id in self.build_data[device]:
                return self.build_data[device][build_id]
        else:
            self.build_data[device] = {}

        build_dir = os.path.join(self.top_dir, device, build_id)
        flash_zip = os.path.join(build_dir, 'flash.zip')
        with zipfile.ZipFile(flash_zip) as zip:
            app_ini = ConfigParser()
            app_ini.readfp(zip.open('system/b2g/application.ini'))
            platform_ini = ConfigParser()
            platform_ini.readfp(zip.open('system/b2g/platform.ini'))

        build_data = self.build_data[device][build_id] = DictObject({
            'app_version': app_ini.get('App', 'version'),
            'app_build_id': app_ini.get('App', 'buildid'),
            'platform_build_id': platform_ini.get('Build', 'buildid'),
            'platform_milestone': platform_ini.get('Build', 'milestone'),
            'complete_mar': os.path.join(build_dir, 'complete.mar'),
            'flash_script': os.path.join(build_dir, 'flash.sh')
        })

        return build_data

class SmokeTestRunner(object):
    DEVICE_TIMEOUT = 30

    def __init__(self, config, b2g, run_dir=None):
        self.config = config
        self.b2g = b2g
        self.run_dir = run_dir or tempfile.mkdtemp()

        update_tools = self.b2g.import_update_tools()
        self.b2g_config = update_tools.B2GConfig()

    def run_b2g_update_test(self, serial, testvars, tests):
        b2g_update_test = os.path.join(marionette_client_dir,
                                       'venv_b2g_update_test.sh')

        if not tests:
            tests = [os.path.join(marionette_client_dir, 'tests',
                                  'update-tests.ini')]

        args = ['bash', b2g_update_test, sys.executable,
                '--homedir', self.b2g.homedir,
                '--address', 'localhost:2828',
                '--type', 'b2g+smoketest',
                '--device', serial,
                '--testvars', testvars]
        args.extend(tests)

        print ' '.join(args)
        subprocess.check_call(args)

    def build_testvars(self, device, start_id, finish_id):
        run_dir = os.path.join(self.run_dir, device, start_id, finish_id)
        if not os.path.exists(run_dir):
            os.makedirs(run_dir)

        start_data = self.config.get_build_data(device, start_id)
        finish_data = self.config.get_build_data(device, finish_id)

        partial_mar = os.path.join(run_dir, 'partial.mar')
        if not os.path.exists(partial_mar):
            build_gecko_mar = os.path.join(self.b2g.update_tools,
                                           'build-gecko-mar.py')
            subprocess.check_call([sys.executable, build_gecko_mar,
                                   '--from', start_data.complete_mar,
                                   '--to', finish_data.complete_mar,
                                   partial_mar])
        finish_data['partial_mar'] = partial_mar

        testvars = os.path.join(run_dir, 'testvars.json')
        if not os.path.exists(testvars):
            open(testvars, 'w').write(json.dumps({
                'start': start_data,
                'finish': finish_data
            }))

        return testvars

    def wait_for_device(self, device):
        for serial in self.config.devices[device].serials:
            proc = subprocess.Popen([self.b2g.adb_path, '-s', serial,
                                     'wait-for-device'])
            def wait_for_adb():
                proc.communicate()

            thread = threading.Thread(target=wait_for_adb)
            thread.start()
            thread.join(self.DEVICE_TIMEOUT)

            if thread.isAlive():
                print >>sys.stderr, '%s device %s is not recognized by ADB, ' \
                                    'trying next device' % (device, serial)
                proc.kill()
                thread.join()
                continue

            return serial
        return None

    def run_smoketests_for_device(self, device, start_id, finish_id, tests):
        testvars = self.build_testvars(device, start_id, finish_id)
        serial = self.wait_for_device(device)
        if not serial:
            raise SmokeTestError('No connected serials for device "%s" could ' \
                                 'be found' % device)

        try:
            self.run_b2g_update_test(serial, testvars, tests)
        except subprocess.CalledProcessError:
            print >>sys.stderr, 'SMOKETEST-FAIL | START=%s | FINISH=%s | ' \
                                'DEVICE=%s/%s | %s' % (start_id, finish_id,
                                                       device, serial, testvars)

    def run_smoketests(self, build_ids, tests):
        build_ids.sort()

        latest_build_id = build_ids.pop(-1)
        for build_id in build_ids:
            for device in self.config.devices:
                self.run_smoketests_for_device(device, build_id,
                                               latest_build_id, tests)
