#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import json
import os
import shutil
import stat
import subprocess
import sys

from ConfigParser import ConfigParser
from smoketest import *

this_dir = os.path.abspath(os.path.dirname(__file__))

def stage_update(device, stage_dir):
    config = SmokeTestConfig(stage_dir)
    if device not in config.devices:
        raise SmokeTestConfigError('Device "%s" not found' % device)

    device_data = config.devices[device]

    b2g_config = b2g.import_update_tools().B2GConfig()
    target_out_dir = os.path.join(b2g.homedir, 'out', 'target', 'product', device)
    app_ini = os.path.join(b2g_config.gecko_objdir, 'dist', 'bin',
                           'application.ini')
    gecko_mar = os.path.join(b2g_config.gecko_objdir, 'dist', 'b2g-update',
                             'b2g-gecko-update.mar')

    build_data = ConfigParser()
    build_data.read(app_ini)
    build_id = build_data.get('App', 'buildid')
    app_version = build_data.get('App', 'version')

    build_dir = os.path.join(config.top_dir, device, build_id)
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    if not os.path.exists(build_dir):
        raise SmokeTestError('Couldn\'t create directory: %s' % build_dir)

    build_flash_fota = os.path.join(b2g.update_tools, 'build-flash-fota.py')

    flash_zip = os.path.join(build_dir, 'flash.zip')

    print 'Building flash zip for device %s, version %s, build %s...' % \
          (device, app_version, build_id)

    subprocess.check_call([sys.executable, build_flash_fota,
        '--system-dir', os.path.join(target_out_dir, 'system'),
        '--system-fs-type', device_data.system_fs_type,
        '--system-location', device_data.system_location,
        '--data-fs-type', device_data.data_fs_type,
        '--data-location', device_data.data_location,
        '--output', flash_zip])

    complete_mar = os.path.join(build_dir, 'complete.mar')
    shutil.copy(gecko_mar, complete_mar)

    flash_template = open(os.path.join(this_dir, 'flash-template.sh')).read()
    flash_script = os.path.join(build_dir, 'flash.sh')
    open(os.path.join(build_dir, 'flash.sh'), 'w').write(flash_template % {
        'flash_zip': flash_zip,
        'sdcard': device_data.sdcard,
        'sdcard_recovery': device_data.sdcard_recovery
    })
    os.chmod(flash_script, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR |
                           stat.S_IRGRP | stat.S_IXGRP |
                           stat.S_IROTH | stat.S_IXOTH)

def main():
    parser = argparse.ArgumentParser(prog='stage-update.py')
    parser.add_argument('device', help='device name for staging')
    parser.add_argument('stage_dir',
        help='directory to stage update and flash script for smoketests')
    args = parser.parse_args()

    try:
        global b2g
        b2g = find_b2g()
    except EnvironmentError, e:
        parser.exit(1, 'This tool must be run while inside the B2G directory, '
                       'or B2G_HOME must be set in the environment.')

    try:
        stage_update(args.device, args.stage_dir)
    except SmokeTestError, e:
        parser.exit(1, str(e))

if __name__ == '__main__':
    main()
