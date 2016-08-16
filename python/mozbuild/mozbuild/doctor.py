# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import subprocess
import sys

import psutil

from distutils.util import strtobool
from distutils.version import LooseVersion
import mozpack.path as mozpath

# Minimum recommended logical processors in system.
PROCESSORS_THRESHOLD = 4

# Minimum recommended total system memory, in gigabytes.
MEMORY_THRESHOLD = 7.4

# Minimum recommended free space on each disk, in gigabytes.
FREESPACE_THRESHOLD = 10

# Latest MozillaBuild version
LATEST_MOZILLABUILD_VERSION = '1.11.0'

DISABLE_LASTACCESS_WIN = '''
Disable the last access time feature?
This improves the speed of file and
directory access by deferring Last Access Time modification on disk by up to an
hour. Backup programs that rely on this feature may be affected.
https://technet.microsoft.com/en-us/library/cc785435.aspx
'''

class Doctor(object):
    def __init__(self, srcdir, objdir, fix):
        self.srcdir = mozpath.normpath(srcdir)
        self.objdir = mozpath.normpath(objdir)
        self.srcdir_mount = self.getmount(self.srcdir)
        self.objdir_mount = self.getmount(self.objdir)
        self.path_mounts = [
            ('srcdir', self.srcdir, self.srcdir_mount),
            ('objdir', self.objdir, self.objdir_mount)
        ]
        self.fix = fix
        self.results = []

    def check_all(self):
        checks = [
            'cpu',
            'memory',
            'storage_freespace',
            'fs_lastaccess',
            'mozillabuild'
        ]
        for check in checks:
            self.report(getattr(self, check))
        good = True
        fixable = False
        denied = False
        for result in self.results:
            if result.get('status') != 'GOOD':
                good = False
            if result.get('fixable', False):
                fixable = True
            if result.get('denied', False):
                denied = True
        if denied:
            print('run "mach doctor --fix" AS ADMIN to re-attempt fixing your system')
        elif False: # elif fixable:
            print('run "mach doctor --fix" as admin to attempt fixing your system')
        return int(not good)

    def getmount(self, path):
        while path != '/' and not os.path.ismount(path):
            path = mozpath.abspath(mozpath.join(path, os.pardir))
        return path

    def prompt_bool(self, prompt, limit=5):
        ''' Prompts the user with prompt and requires a boolean value. '''
        valid = False
        while not valid and limit > 0:
            try:
                choice = strtobool(raw_input(prompt + '[Y/N]\n'))
                valid = True
            except ValueError:
                print("ERROR! Please enter a valid option!")
                limit -= 1

        if limit > 0:
            return choice
        else:
            raise Exception("Error! Reached max attempts of entering option.")

    def report(self, results):
        # Handle single dict result or list of results.
        if isinstance(results, dict):
            results = [results]
        for result in results:
            status = result.get('status', 'UNSURE')
            if status == 'SKIPPED':
                continue
            self.results.append(result)
            print('%s...\t%s\n' % (
                   result.get('desc', ''),
                   status
                )
            ).expandtabs(40)

    @property
    def platform(self):
        platform = getattr(self, '_platform', None)
        if not platform:
            platform = sys.platform
            while platform[-1].isdigit():
                platform = platform[:-1]
            setattr(self, '_platform', platform)
        return platform

    @property
    def cpu(self):
        cpu_count = psutil.cpu_count()
        if cpu_count < PROCESSORS_THRESHOLD:
            status = 'BAD'
            desc = '%d logical processors detected, <%d' % (
                cpu_count, PROCESSORS_THRESHOLD
            )
        else:
            status = 'GOOD'
            desc = '%d logical processors detected, >=%d' % (
                cpu_count, PROCESSORS_THRESHOLD
            )
        return {'status': status, 'desc': desc}

    @property
    def memory(self):
        memory = psutil.virtual_memory().total
        # Convert to gigabytes.
        memory_GB = memory / 1024**3.0
        if memory_GB < MEMORY_THRESHOLD:
            status = 'BAD'
            desc = '%.1fGB of physical memory, <%.1fGB' % (
                memory_GB, MEMORY_THRESHOLD
            )
        else:
            status = 'GOOD'
            desc = '%.1fGB of physical memory, >%.1fGB' % (
                memory_GB, MEMORY_THRESHOLD
            )
        return {'status': status, 'desc': desc}

    @property
    def storage_freespace(self):
        results = []
        desc = ''
        mountpoint_line = self.srcdir_mount != self.objdir_mount
        for (purpose, path, mount) in self.path_mounts:
            desc += '%s = %s\n' % (purpose, path)
            if not mountpoint_line:
                mountpoint_line = True
                continue
            try:
                usage = psutil.disk_usage(mount)
                freespace, size = usage.free, usage.total
                freespace_GB = freespace / 1024**3
                size_GB = size / 1024**3
                if freespace_GB < FREESPACE_THRESHOLD:
                    status = 'BAD'
                    desc += 'mountpoint = %s\n%dGB of %dGB free, <%dGB' % (
                        mount, freespace_GB, size_GB, FREESPACE_THRESHOLD
                    )
                else:
                    status = 'GOOD'
                    desc += 'mountpoint = %s\n%dGB of %dGB free, >=%dGB' % (
                        mount, freespace_GB, size_GB, FREESPACE_THRESHOLD
                    )
            except OSError:
                status = 'UNSURE'
                desc += 'path invalid'
            results.append({'status': status, 'desc': desc})
        return results

    @property
    def fs_lastaccess(self):
        results = []
        if self.platform == 'win':
            fixable = False
            denied = False
            # See 'fsutil behavior':
            # https://technet.microsoft.com/en-us/library/cc785435.aspx
            try:
                command = 'fsutil behavior query disablelastaccess'.split(' ')
                fsutil_output = subprocess.check_output(command)
                disablelastaccess = int(fsutil_output.partition('=')[2][1])
            except subprocess.CalledProcessError:
                disablelastaccess = -1
                status = 'UNSURE'
                desc = 'unable to check lastaccess behavior'
            if disablelastaccess == 1:
                status = 'GOOD'
                desc = 'lastaccess disabled systemwide'
            elif disablelastaccess == 0:
                if False: # if self.fix:
                    choice = self.prompt_bool(DISABLE_LASTACCESS_WIN)
                    if not choice:
                        return {'status': 'BAD, NOT FIXED',
                                'desc': 'lastaccess enabled systemwide'}
                    try:
                        command = 'fsutil behavior set disablelastaccess 1'.split(' ')
                        fsutil_output = subprocess.check_output(command)
                        status = 'GOOD, FIXED'
                        desc = 'lastaccess disabled systemwide'
                    except subprocess.CalledProcessError, e:
                        desc = 'lastaccess enabled systemwide'
                        if e.output.find('denied') != -1:
                            status = 'BAD, FIX DENIED'
                            denied = True
                        else:
                            status = 'BAD, NOT FIXED'
                else:
                    status = 'BAD, FIXABLE'
                    desc = 'lastaccess enabled'
                    fixable = True
            results.append({'status': status, 'desc': desc, 'fixable': fixable,
                            'denied': denied})
        elif self.platform in ['darwin', 'freebsd', 'linux', 'openbsd']:
            common_mountpoint = self.srcdir_mount == self.objdir_mount
            for (purpose, path, mount) in self.path_mounts:
                results.append(self.check_mount_lastaccess(mount))
                if common_mountpoint:
                    break
        else:
            results.append({'status': 'SKIPPED'})
        return results

    def check_mount_lastaccess(self, mount):
        partitions = psutil.disk_partitions()
        atime_opts = {'atime', 'noatime', 'relatime', 'norelatime'}
        option = ''
        for partition in partitions:
            if partition.mountpoint == mount:
                mount_opts = set(partition.opts.split(','))
                intersection = list(atime_opts & mount_opts)
                if len(intersection) == 1:
                    option = intersection[0]
                break
        if not option:
            status = 'BAD'
            if self.platform == 'linux':
                option = 'noatime/relatime'
            else:
                option = 'noatime'
            desc = '%s has no explicit %s mount option' % (
                mount, option
            )
        elif option == 'atime' or option == 'norelatime':
            status = 'BAD'
            desc = '%s has %s mount option' % (
                mount, option
            )
        elif option == 'noatime' or option == 'relatime':
            status = 'GOOD'
            desc = '%s has %s mount option' % (
                mount, option
            )
        return {'status': status, 'desc': desc}

    @property
    def mozillabuild(self):
        if self.platform != 'win':
            return {'status': 'SKIPPED'}
        MOZILLABUILD = mozpath.normpath(os.environ.get('MOZILLABUILD', ''))
        if not MOZILLABUILD or not os.path.exists(MOZILLABUILD):
            return {'desc': 'not running under MozillaBuild'}
        try:
            with open(mozpath.join(MOZILLABUILD, 'VERSION'), 'r') as fh:
                version = fh.readline()
            if not version:
                raise ValueError()
            if LooseVersion(version) < LooseVersion(LATEST_MOZILLABUILD_VERSION):
                status = 'BAD'
                desc = 'MozillaBuild %s in use, <%s' % (
                    version, LATEST_MOZILLABUILD_VERSION
                )
            else:
                status = 'GOOD'
                desc = 'MozillaBuild %s in use' % version
        except (IOError, ValueError):
            status = 'UNSURE'
            desc = 'MozillaBuild version not found'
        return {'status': status, 'desc': desc}
