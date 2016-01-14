# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import getpass
import json
import logging
import os
import platform
import subprocess
import sys
import time
import which

from collections import (
    namedtuple,
    OrderedDict,
)

try:
    import psutil
except Exception:
    psutil = None

from mozsystemmonitor.resourcemonitor import SystemResourceMonitor

from ..base import MozbuildObject

from ..compilation.warnings import (
    WarningsCollector,
    WarningsDatabase,
)

from textwrap import TextWrapper

INSTALL_TESTS_CLOBBER = ''.join([TextWrapper().fill(line) + '\n' for line in
'''
The build system was unable to install tests because the CLOBBER file has \
been updated. This means if you edited any test files, your changes may not \
be picked up until a full/clobber build is performed.

The easiest and fastest way to perform a clobber build is to run:

 $ mach clobber
 $ mach build

If you did not modify any test files, it is safe to ignore this message \
and proceed with running tests. To do this run:

 $ touch {clobber_file}
'''.splitlines()])



BuildOutputResult = namedtuple('BuildOutputResult',
    ('warning', 'state_changed', 'for_display'))


class TierStatus(object):
    """Represents the state and progress of tier traversal.

    The build system is organized into linear phases called tiers. Each tier
    executes in the order it was defined, 1 at a time.
    """

    def __init__(self, resources):
        """Accepts a SystemResourceMonitor to record results against."""
        self.tiers = OrderedDict()
        self.tier_status = OrderedDict()
        self.resources = resources

    def set_tiers(self, tiers):
        """Record the set of known tiers."""
        for tier in tiers:
            self.tiers[tier] = dict(
                begin_time=None,
                finish_time=None,
                duration=None,
            )
            self.tier_status[tier] = None

    def begin_tier(self, tier):
        """Record that execution of a tier has begun."""
        self.tier_status[tier] = 'active'
        t = self.tiers[tier]
        # We should ideally use a monotonic clock here. Unfortunately, we won't
        # have one until Python 3.
        t['begin_time'] = time.time()
        self.resources.begin_phase(tier)

    def finish_tier(self, tier):
        """Record that execution of a tier has finished."""
        self.tier_status[tier] = 'finished'
        t = self.tiers[tier]
        t['finish_time'] = time.time()
        t['duration'] = self.resources.finish_phase(tier)

    def tiered_resource_usage(self):
        """Obtains an object containing resource usage for tiers.

        The returned object is suitable for serialization.
        """
        o = []

        for tier, state in self.tiers.items():
            t_entry = dict(
                name=tier,
                start=state['begin_time'],
                end=state['finish_time'],
                duration=state['duration'],
            )

            self.add_resources_to_dict(t_entry, phase=tier)

            o.append(t_entry)

        return o

    def add_resources_to_dict(self, entry, start=None, end=None, phase=None):
        """Helper function to append resource information to a dict."""
        cpu_percent = self.resources.aggregate_cpu_percent(start=start,
            end=end, phase=phase, per_cpu=False)
        cpu_times = self.resources.aggregate_cpu_times(start=start, end=end,
            phase=phase, per_cpu=False)
        io = self.resources.aggregate_io(start=start, end=end, phase=phase)

        if cpu_percent is None:
            return entry

        entry['cpu_percent'] = cpu_percent
        entry['cpu_times'] = list(cpu_times)
        entry['io'] = list(io)

        return entry

    def add_resource_fields_to_dict(self, d):
        for usage in self.resources.range_usage():
            cpu_times = self.resources.aggregate_cpu_times(per_cpu=False)

            d['cpu_times_fields'] = list(cpu_times._fields)
            d['io_fields'] = list(usage.io._fields)
            d['virt_fields'] = list(usage.virt._fields)
            d['swap_fields'] = list(usage.swap._fields)

            return d


class BuildMonitor(MozbuildObject):
    """Monitors the output of the build."""

    def init(self, warnings_path):
        """Create a new monitor.

        warnings_path is a path of a warnings database to use.
        """
        self._warnings_path = warnings_path
        self.resources = SystemResourceMonitor(poll_interval=1.0)
        self._resources_started = False

        self.tiers = TierStatus(self.resources)

        self.warnings_database = WarningsDatabase()
        if os.path.exists(warnings_path):
            try:
                self.warnings_database.load_from_file(warnings_path)
            except ValueError:
                os.remove(warnings_path)

        self._warnings_collector = WarningsCollector(
            database=self.warnings_database, objdir=self.topobjdir)

        self.build_objects = []

    def start(self):
        """Record the start of the build."""
        self.start_time = time.time()
        self._finder_start_cpu = self._get_finder_cpu_usage()

    def start_resource_recording(self):
        # This should be merged into start() once bug 892342 lands.
        self.resources.start()
        self._resources_started = True

    def on_line(self, line):
        """Consume a line of output from the build system.

        This will parse the line for state and determine whether more action is
        needed.

        Returns a BuildOutputResult instance.

        In this named tuple, warning will be an object describing a new parsed
        warning. Otherwise it will be None.

        state_changed indicates whether the build system changed state with
        this line. If the build system changed state, the caller may want to
        query this instance for the current state in order to update UI, etc.

        for_display is a boolean indicating whether the line is relevant to the
        user. This is typically used to filter whether the line should be
        presented to the user.
        """
        if line.startswith('BUILDSTATUS'):
            args = line.split()[1:]

            action = args.pop(0)
            update_needed = True

            if action == 'TIERS':
                self.tiers.set_tiers(args)
                update_needed = False
            elif action == 'TIER_START':
                tier = args[0]
                self.tiers.begin_tier(tier)
            elif action == 'TIER_FINISH':
                tier, = args
                self.tiers.finish_tier(tier)
            elif action == 'OBJECT_FILE':
                self.build_objects.append(args[0])
                update_needed = False
            else:
                raise Exception('Unknown build status: %s' % action)

            return BuildOutputResult(None, update_needed, False)

        warning = None

        try:
            warning = self._warnings_collector.process_line(line)
        except:
            pass

        return BuildOutputResult(warning, False, True)

    def finish(self, record_usage=True):
        """Record the end of the build."""
        self.end_time = time.time()

        if self._resources_started:
            self.resources.stop()

        self._finder_end_cpu = self._get_finder_cpu_usage()
        self.elapsed = self.end_time - self.start_time

        self.warnings_database.prune()
        self.warnings_database.save_to_file(self._warnings_path)

        if not record_usage:
            return

        try:
            usage = self.record_resource_usage()
            if not usage:
                return

            with open(self._get_state_filename('build_resources.json'), 'w') as fh:
                json.dump(usage, fh, indent=2)
        except Exception as e:
            self.log(logging.WARNING, 'build_resources_error',
                {'msg': str(e)},
                'Exception when writing resource usage file: {msg}')

    def _get_finder_cpu_usage(self):
        """Obtain the CPU usage of the Finder app on OS X.

        This is used to detect high CPU usage.
        """
        if not sys.platform.startswith('darwin'):
            return None

        if not psutil:
            return None

        for proc in psutil.process_iter():
            if proc.name != 'Finder':
                continue

            if proc.username != getpass.getuser():
                continue

            # Try to isolate system finder as opposed to other "Finder"
            # processes.
            if not proc.exe.endswith('CoreServices/Finder.app/Contents/MacOS/Finder'):
                continue

            return proc.get_cpu_times()

        return None

    def have_high_finder_usage(self):
        """Determine whether there was high Finder CPU usage during the build.

        Returns True if there was high Finder CPU usage, False if there wasn't,
        or None if there is nothing to report.
        """
        if not self._finder_start_cpu:
            return None, None

        # We only measure if the measured range is sufficiently long.
        if self.elapsed < 15:
            return None, None

        if not self._finder_end_cpu:
            return None, None

        start = self._finder_start_cpu
        end = self._finder_end_cpu

        start_total = start.user + start.system
        end_total = end.user + end.system

        cpu_seconds = end_total - start_total

        # If Finder used more than 25% of 1 core during the build, report an
        # error.
        finder_percent = cpu_seconds / self.elapsed * 100

        return finder_percent > 25, finder_percent

    def have_excessive_swapping(self):
        """Determine whether there was excessive swapping during the build.

        Returns a tuple of (excessive, swap_in, swap_out). All values are None
        if no swap information is available.
        """
        if not self.have_resource_usage:
            return None, None, None

        swap_in = sum(m.swap.sin for m in self.resources.measurements)
        swap_out = sum(m.swap.sout for m in self.resources.measurements)

        # The threshold of 1024 MB has been arbitrarily chosen.
        #
        # Choosing a proper value that is ideal for everyone is hard. We will
        # likely iterate on the logic until people are generally satisfied.
        # If a value is too low, the eventual warning produced does not carry
        # much meaning. If the threshold is too high, people may not see the
        # warning and the warning will thus be ineffective.
        excessive = swap_in > 512 * 1048576 or swap_out > 512 * 1048576
        return excessive, swap_in, swap_out

    @property
    def have_resource_usage(self):
        """Whether resource usage is available."""
        return self.resources.start_time is not None

    def record_resource_usage(self):
        """Record the resource usage of this build.

        We write a log message containing a high-level summary. We also produce
        a data structure containing the low-level resource usage information.
        This data structure can e.g. be serialized into JSON and saved for
        subsequent analysis.

        If no resource usage is available, None is returned.
        """
        if not self.have_resource_usage:
            return None

        cpu_percent = self.resources.aggregate_cpu_percent(phase=None,
            per_cpu=False)
        cpu_times = self.resources.aggregate_cpu_times(phase=None,
            per_cpu=False)
        io = self.resources.aggregate_io(phase=None)

        self._log_resource_usage('Overall system resources', 'resource_usage',
            self.end_time - self.start_time, cpu_percent, cpu_times, io)

        excessive, sin, sout = self.have_excessive_swapping()
        if excessive is not None and (sin or sout):
            sin /= 1048576
            sout /= 1048576
            self.log(logging.WARNING, 'swap_activity',
                {'sin': sin, 'sout': sout},
                'Swap in/out (MB): {sin}/{sout}')

        o = dict(
            version=2,
            argv=sys.argv,
            start=self.start_time,
            end=self.end_time,
            duration=self.end_time - self.start_time,
            resources=[],
            cpu_percent=cpu_percent,
            cpu_times=cpu_times,
            io=io,
            objects=self.build_objects
        )

        o['tiers'] = self.tiers.tiered_resource_usage()

        self.tiers.add_resource_fields_to_dict(o)

        for usage in self.resources.range_usage():
            cpu_percent = self.resources.aggregate_cpu_percent(usage.start,
                usage.end, per_cpu=False)
            cpu_times = self.resources.aggregate_cpu_times(usage.start,
                usage.end, per_cpu=False)

            entry = dict(
                start=usage.start,
                end=usage.end,
                virt=list(usage.virt),
                swap=list(usage.swap),
            )

            self.tiers.add_resources_to_dict(entry, start=usage.start,
                    end=usage.end)

            o['resources'].append(entry)

        # TODO: it would be nice to collect data on the storage device as well
        o['system'] = dict(
            architecture=list(platform.architecture()),
            logical_cpu_count=psutil.cpu_count(),
            machine=platform.machine(),
            physical_cpu_count=psutil.cpu_count(logical=False),
            python_version=platform.python_version(),
            release=platform.release(),
            system=platform.system(),
            swap_total=psutil.swap_memory()[0],
            version=platform.version(),
            vmem_total=psutil.virtual_memory()[0],
        )

        if platform.system() == 'Linux':
            dist = list(platform.linux_distribution())
            o['system']['linux_distribution'] = dist
        elif platform.system() == 'Windows':
            win32_ver=list((platform.win32_ver())),
            o['system']['win32_ver'] = win32_ver
        elif platform.system() == 'Darwin':
            # mac version is a special Cupertino snowflake
            r, v, m = platform.mac_ver()
            o['system']['mac_ver'] = [r, list(v), m]

        return o

    def _log_resource_usage(self, prefix, m_type, duration, cpu_percent,
        cpu_times, io, extra_params={}):

        params = dict(
            duration=duration,
            cpu_percent=cpu_percent,
            io_reads=io.read_count,
            io_writes=io.write_count,
            io_read_bytes=io.read_bytes,
            io_write_bytes=io.write_bytes,
            io_read_time=io.read_time,
            io_write_time=io.write_time,
        )

        params.update(extra_params)

        message = prefix + ' - Wall time: {duration:.0f}s; ' \
            'CPU: {cpu_percent:.0f}%; ' \
            'Read bytes: {io_read_bytes}; Write bytes: {io_write_bytes}; ' \
            'Read time: {io_read_time}; Write time: {io_write_time}'

        self.log(logging.WARNING, m_type, params, message)

    def ccache_stats(self):
        ccache_stats = None

        try:
            ccache = which.which('ccache')
            output = subprocess.check_output([ccache, '-s'])
            ccache_stats = CCacheStats(output)
        except which.WhichError:
            pass
        except ValueError as e:
            self.log(logging.WARNING, 'ccache', {'msg': str(e)}, '{msg}')

        return ccache_stats


class CCacheStats(object):
    """Holds statistics from ccache.

    Instances can be subtracted from each other to obtain differences.
    print() or str() the object to show a ``ccache -s`` like output
    of the captured stats.

    """
    STATS_KEYS = [
        # (key, description)
        # Refer to stats.c in ccache project for all the descriptions.
        ('cache_hit_direct', 'cache hit (direct)'),
        ('cache_hit_preprocessed', 'cache hit (preprocessed)'),
        ('cache_miss', 'cache miss'),
        ('link', 'called for link'),
        ('preprocessing', 'called for preprocessing'),
        ('multiple', 'multiple source files'),
        ('stdout', 'compiler produced stdout'),
        ('no_output', 'compiler produced no output'),
        ('empty_output', 'compiler produced empty output'),
        ('failed', 'compile failed'),
        ('error', 'ccache internal error'),
        ('preprocessor_error', 'preprocessor error'),
        ('cant_use_pch', "can't use precompiled header"),
        ('compiler_missing', "couldn't find the compiler"),
        ('cache_file_missing', 'cache file missing'),
        ('bad_args', 'bad compiler arguments'),
        ('unsupported_lang', 'unsupported source language'),
        ('compiler_check_failed', 'compiler check failed'),
        ('autoconf', 'autoconf compile/link'),
        ('unsupported_compiler_option', 'unsupported compiler option'),
        ('out_stdout', 'output to stdout'),
        ('out_device', 'output to a non-regular file'),
        ('no_input', 'no input file'),
        ('bad_extra_file', 'error hashing extra file'),
        ('cache_files', 'files in cache'),
        ('cache_size', 'cache size'),
        ('cache_max_size', 'max cache size'),
    ]

    DIRECTORY_DESCRIPTION = "cache directory"
    PRIMARY_CONFIG_DESCRIPTION = "primary config"
    SECONDARY_CONFIG_DESCRIPTION = "secondary config      (readonly)"
    ABSOLUTE_KEYS = {'cache_files', 'cache_size', 'cache_max_size'}
    FORMAT_KEYS = {'cache_size', 'cache_max_size'}

    GiB = 1024 ** 3
    MiB = 1024 ** 2
    KiB = 1024

    def __init__(self, output=None):
        """Construct an instance from the output of ccache -s."""
        self._values = {}
        self.cache_dir = ""
        self.primary_config = ""
        self.secondary_config = ""

        if not output:
            return

        for line in output.splitlines():
            line = line.strip()
            if line:
                self._parse_line(line)

    def _parse_line(self, line):
        if line.startswith(self.DIRECTORY_DESCRIPTION):
            self.cache_dir = self._strip_prefix(line, self.DIRECTORY_DESCRIPTION)
        elif line.startswith(self.PRIMARY_CONFIG_DESCRIPTION):
            self.primary_config = self._strip_prefix(
                line, self.PRIMARY_CONFIG_DESCRIPTION)
        elif line.startswith(self.SECONDARY_CONFIG_DESCRIPTION):
            self.secondary_config = self._strip_prefix(
                line, self.SECONDARY_CONFIG_DESCRIPTION)
        else:
            for stat_key, stat_description in self.STATS_KEYS:
                if line.startswith(stat_description):
                    raw_value = self._strip_prefix(line, stat_description)
                    self._values[stat_key] = self._parse_value(raw_value)
                    break
            else:
                raise ValueError('Failed to parse ccache stats output: %s' % line)

    @staticmethod
    def _strip_prefix(line, prefix):
        return line[len(prefix):].strip() if line.startswith(prefix) else line

    @staticmethod
    def _parse_value(raw_value):
        value = raw_value.split()
        unit = ''
        if len(value) == 1:
            numeric = value[0]
        elif len(value) == 2:
            numeric, unit = value
        else:
            raise ValueError('Failed to parse ccache stats value: %s' % raw_value)

        if '.' in numeric:
            numeric = float(numeric)
        else:
            numeric = int(numeric)

        if unit in ('GB', 'Gbytes'):
            unit = CCacheStats.GiB
        elif unit in ('MB', 'Mbytes'):
            unit = CCacheStats.MiB
        elif unit in ('KB', 'Kbytes'):
            unit = CCacheStats.KiB
        else:
            unit = 1

        return int(numeric * unit)

    def hit_rate_message(self):
        return 'ccache (direct) hit rate: {:.1%}; (preprocessed) hit rate: {:.1%}; miss rate: {:.1%}'.format(*self.hit_rates())

    def hit_rates(self):
        direct = self._values['cache_hit_direct']
        preprocessed = self._values['cache_hit_preprocessed']
        miss = self._values['cache_miss']
        total = float(direct + preprocessed + miss)

        if total > 0:
            direct /= total
            preprocessed /= total
            miss /= total

        return (direct, preprocessed, miss)

    def __sub__(self, other):
        result = CCacheStats()
        result.cache_dir = self.cache_dir

        for k, prefix in self.STATS_KEYS:
            if k not in self._values and k not in other._values:
                continue

            our_value = self._values.get(k, 0)
            other_value = other._values.get(k, 0)

            if k in self.ABSOLUTE_KEYS:
                result._values[k] = our_value
            else:
                result._values[k] = our_value - other_value

        return result

    def __str__(self):
        LEFT_ALIGN = 34
        lines = []

        if self.cache_dir:
            lines.append('%s%s' % (self.DIRECTORY_DESCRIPTION.ljust(LEFT_ALIGN),
                                   self.cache_dir))

        for stat_key, stat_description in self.STATS_KEYS:
            if stat_key not in self._values:
                continue

            value = self._values[stat_key]

            if stat_key in self.FORMAT_KEYS:
                value = '%15s' % self._format_value(value)
            else:
                value = '%8u' % value

            lines.append('%s%s' % (stat_description.ljust(LEFT_ALIGN), value))

        return '\n'.join(lines)

    def __nonzero__(self):
        relative_values = [v for k, v in self._values.items()
                           if k not in self.ABSOLUTE_KEYS]
        return (all(v >= 0 for v in relative_values) and
                any(v > 0 for v in relative_values))

    @staticmethod
    def _format_value(v):
        if v > CCacheStats.GiB:
            return '%.1f Gbytes' % (float(v) / CCacheStats.GiB)
        elif v > CCacheStats.MiB:
            return '%.1f Mbytes' % (float(v) / CCacheStats.MiB)
        else:
            return '%.1f Kbytes' % (float(v) / CCacheStats.KiB)


class BuildDriver(MozbuildObject):
    """Provides a high-level API for build actions."""

    def install_tests(self, remove=True):
        """Install test files (through manifest)."""

        if self.is_clobber_needed():
            print(INSTALL_TESTS_CLOBBER.format(
                  clobber_file=os.path.join(self.topobjdir, 'CLOBBER')))
            sys.exit(1)

        env = {}
        if not remove:
            env[b'NO_REMOVE'] = b'1'

        self._run_make(target='install-tests', append_env=env, pass_thru=True,
            print_directory=False)
