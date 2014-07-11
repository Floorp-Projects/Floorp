#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This scripts sets up a virtualenv and installs TPS into it.
It's probably best to specify a path NOT inside the repo, otherwise
all the virtualenv files will show up in e.g. hg status.
"""

import optparse
import os
import shutil
import subprocess
import sys
import urllib2
import zipfile


here = os.path.dirname(os.path.abspath(__file__))
usage_message = """
***********************************************************************

To run TPS, activate the virtualenv using:
    source {TARGET}/{BIN_NAME}

To change your TPS config, please edit the file:
    {TARGET}/config.json

To execute tps use:
    runtps --binary=/path/to/firefox

See runtps --help for all options

***********************************************************************
"""

# Link to the folder, which contains the zip archives of virtualenv
URL_VIRTUALENV = 'https://codeload.github.com/pypa/virtualenv/zip/'
VERSION_VIRTUALENV = '1.11.6'


if sys.platform == 'win32':
    bin_name = os.path.join('Scripts', 'activate.bat')
    activate_env = os.path.join('Scripts', 'activate_this.py')
    python_env = os.path.join('Scripts', 'python.exe')
else:
    bin_name = os.path.join('bin', 'activate')
    activate_env = os.path.join('bin', 'activate_this.py')
    python_env = os.path.join('bin', 'python')


def download(url, target):
    """Downloads the specified url to the given target."""
    response = urllib2.urlopen(url)
    with open(target, 'wb') as f:
        f.write(response.read())

    return target


def setup_virtualenv(target, python_bin=None):
    script_path = os.path.join(here, 'virtualenv-%s' % VERSION_VIRTUALENV,
                               'virtualenv.py')

    print 'Downloading virtualenv %s' % VERSION_VIRTUALENV
    zip_path = download(URL_VIRTUALENV + VERSION_VIRTUALENV,
                        os.path.join(here, 'virtualenv.zip'))

    try:
        with zipfile.ZipFile(zip_path, 'r') as f:
            f.extractall(here)

        print 'Creating new virtual environment'
        cmd_args = [sys.executable, script_path, target]

        if python_bin:
            cmd_args.extend(['-p', python_bin])

        subprocess.check_call(cmd_args)
    finally:
        try:
            os.remove(zip_path)
        except OSError:
            pass

        shutil.rmtree(os.path.dirname(script_path), ignore_errors=True)


def update_configfile(source, target, replacements):
    lines = []

    with open(source) as config:
        for line in config:
            for source_string, target_string in replacements.iteritems():
                if target_string:
                    line = line.replace(source_string, target_string)
            lines.append(line)

    with open(target, 'w') as config:
        for line in lines:
            config.write(line)


def main():
    parser = optparse.OptionParser('Usage: %prog [options] path_to_venv')
    parser.add_option('--password',
                      type='string',
                      dest='password',
                      metavar='FX_ACCOUNT_PASSWORD',
                      default=None,
                      help='The Firefox Account password.')
    parser.add_option('-p', '--python',
                      type='string',
                      dest='python',
                      metavar='PYTHON_BIN',
                      default=None,
                      help='The Python interpreter to use.')
    parser.add_option('--sync-passphrase',
                      type='string',
                      dest='sync_passphrase',
                      metavar='SYNC_ACCOUNT_PASSPHRASE',
                      default=None,
                      help='The old Firefox Sync account passphrase.')
    parser.add_option('--sync-password',
                      type='string',
                      dest='sync_password',
                      metavar='SYNC_ACCOUNT_PASSWORD',
                      default=None,
                      help='The old Firefox Sync account password.')
    parser.add_option('--sync-username',
                      type='string',
                      dest='sync_username',
                      metavar='SYNC_ACCOUNT_USERNAME',
                      default=None,
                      help='The old Firefox Sync account username.')
    parser.add_option('--username',
                      type='string',
                      dest='username',
                      metavar='FX_ACCOUNT_USERNAME',
                      default=None,
                      help='The Firefox Account username.')

    (options, args) = parser.parse_args(args=None, values=None)

    if len(args) != 1:
         parser.error('Path to the environment has to be specified')
    target = args[0]
    assert(target)

    setup_virtualenv(target, python_bin=options.python)

    # Activate tps environment
    tps_env = os.path.join(target, activate_env)
    execfile(tps_env, dict(__file__=tps_env))

    # Install TPS in environment
    subprocess.check_call([os.path.join(target, python_env),
                           os.path.join(here, 'setup.py'), 'install'])

    # Get the path to tests and extensions directory by checking check where
    # the tests and extensions directories are located
    sync_dir = os.path.abspath(os.path.join(here, '..', '..', 'services',
                                            'sync'))
    if os.path.exists(sync_dir):
        testdir = os.path.join(sync_dir, 'tests', 'tps')
        extdir = os.path.join(sync_dir, 'tps', 'extensions')
    else:
        testdir = os.path.join(here, 'tests')
        extdir = os.path.join(here, 'extensions')

    update_configfile(os.path.join(here, 'config', 'config.json.in'),
                      os.path.join(target, 'config.json'),
                      replacements={
                      '__TESTDIR__': testdir.replace('\\','/'),
                      '__EXTENSIONDIR__': extdir.replace('\\','/'),
                      '__FX_ACCOUNT_USERNAME__': options.username,
                      '__FX_ACCOUNT_PASSWORD__': options.password,
                      '__SYNC_ACCOUNT_USERNAME__': options.sync_username,
                      '__SYNC_ACCOUNT_PASSWORD__': options.sync_password,
                      '__SYNC_ACCOUNT_PASSPHRASE__': options.sync_passphrase})

    if not (options.username and options.password):
        print '\nFirefox Account credentials not specified.'
    if not (options.sync_username and options.sync_password and options.passphrase):
        print '\nFirefox Sync account credentials not specified.'

    # Print the user instructions
    print usage_message.format(TARGET=target,
                               BIN_NAME=bin_name)

if __name__ == "__main__":
    main()
