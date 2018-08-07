# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

'''
This file contains a voluptuous schema definition for build system telemetry.
'''

import platform
import sys
from voluptuous import (
    Any,
    Optional,
    Required,
    Schema,
)
from voluptuous.validators import Datetime

from .base import (
    BuildEnvironmentNotFoundException,
)
from .configure.constants import CompilerType

schema = Schema({
    Required('client_id', description='A UUID to uniquely identify a client'): basestring,
    Required('time', description='Time at which this event happened'): Datetime(),
    Required('command', description='The mach command that was invoked'): basestring,
    Required('argv', description=(
        'Full mach commandline. ' +
        'If the commandline contains absolute paths they will be sanitized.')): [basestring],
    Required('success', description='true if the command succeeded'): bool,
    Optional('exception', description=(
        'If a Python exception was encountered during the execution of the command, ' +
        'this value contains the result of calling `repr` on the exception object.')): basestring,
    Optional('file_types_changed', description=(
        'This array contains a list of objects with {ext, count} properties giving the count ' +
        'of files changed since the last invocation grouped by file type')): [
            {
                Required('ext', description='File extension'): basestring,
                Required('count', description='Count of changed files with this extension'): int,
            }
        ],
    Required('duration_ms', description='Command duration in milliseconds'): int,
    Required('build_opts', description='Selected build options'): {
        Optional('compiler', description='The compiler type in use (CC_TYPE)'):
            Any(*CompilerType.POSSIBLE_VALUES),
        Optional('artifact', description='true if --enable-artifact-builds'): bool,
        Optional('debug', description='true if build is debug (--enable-debug)'): bool,
        Optional('opt', description='true if build is optimized (--enable-optimize)'): bool,
        Optional('ccache', description='true if ccache is in use (--with-ccache)'): bool,
        Optional('sccache', description='true if ccache in use is sccache'): bool,
        Optional('icecream', description='true if icecream in use'): bool,
    },
    Required('system'): {
        # We don't need perfect granularity here.
        Required('os', description='Operating system'): Any('windows', 'macos', 'linux', 'other'),
        Optional('cpu_brand', description='CPU brand string from CPUID'): basestring,
        Optional('logical_cores', description='Number of logical CPU cores present'): int,
        Optional('physical_cores', description='Number of physical CPU cores present'): int,
        Optional('memory_gb', description='System memory in GB'): int,
        Optional('drive_is_ssd',
                 description='true if the source directory is on a solid-state disk'): bool,
        Optional('virtual_machine',
                 description='true if the OS appears to be running in a virtual machine'): bool,
    },
})


def gather_telemetry(monitor, mach_context, substs, ccache_diff):
    if monitor.have_resource_usage:
        telemetry_handler = getattr(mach_context,
                                    'telemetry_handler', None)
        data = monitor.get_resource_usage()

        # Record build configuration data. For now, we cherry pick
        # items we need rather than grabbing everything, in order
        # to avoid accidentally disclosing PII.
        data['substs'] = {}
        try:
            for key in ['MOZ_ARTIFACT_BUILDS', 'MOZ_USING_CCACHE', 'MOZ_USING_SCCACHE']:
                value = substs.get(key, False)
                data['substs'][key] = value
        except BuildEnvironmentNotFoundException:
            pass

        # Grab ccache stats if available. We need to be careful not
        # to capture information that can potentially identify the
        # user (such as the cache location)
        if ccache_diff:
            data['ccache'] = {}
            for key in [key[0] for key in ccache_diff.STATS_KEYS]:
                try:
                    data['ccache'][key] = ccache_diff._values[key]
                except KeyError:
                    pass

        # Add common metadata to help submit sorted data later on.
        data['argv'] = sys.argv
        data.setdefault('system', {}).update(dict(
            architecture=list(platform.architecture()),
            machine=platform.machine(),
            python_version=platform.python_version(),
            release=platform.release(),
            system=platform.system(),
            version=platform.version(),
        ))

        if platform.system() == 'Linux':
            dist = list(platform.linux_distribution())
            data['system']['linux_distribution'] = dist
        elif platform.system() == 'Windows':
            win32_ver = list((platform.win32_ver())),
            data['system']['win32_ver'] = win32_ver
        elif platform.system() == 'Darwin':
            # mac version is a special Cupertino snowflake
            r, v, m = platform.mac_ver()
            data['system']['mac_ver'] = [r, list(v), m]

        if telemetry_handler:
            telemetry_handler(mach_context, data)
