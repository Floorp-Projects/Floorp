# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import errno
import getpass
import io
import json
import logging
import os
import subprocess
import sys
import time
import which

from collections import (
    Counter,
    namedtuple,
    OrderedDict,
)
from textwrap import (
    TextWrapper,
)

try:
    import psutil
except Exception:
    psutil = None

from mach.mixin.logging import LoggingMixin
from mozsystemmonitor.resourcemonitor import SystemResourceMonitor
from mozterm.widgets import Footer

import mozpack.path as mozpath

from .clobber import (
    Clobberer,
)
from ..base import (
    BuildEnvironmentNotFoundException,
    MozbuildObject,
)
from ..backend import (
    get_backend_class,
)
from ..testing import (
    install_test_files,
)
from ..compilation.warnings import (
    WarningsCollector,
    WarningsDatabase,
)
from ..shellutil import (
    quote as shell_quote,
)
from ..util import (
    FileAvoidWrite,
    mkdir,
    resolve_target_to_make,
)


FINDER_SLOW_MESSAGE = '''
===================
PERFORMANCE WARNING

The OS X Finder application (file indexing used by Spotlight) used a lot of CPU
during the build - an average of %f%% (100%% is 1 core). This made your build
slower.

Consider adding ".noindex" to the end of your object directory name to have
Finder ignore it. Or, add an indexing exclusion through the Spotlight System
Preferences.
===================
'''.strip()


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

        # Contains warnings unique to this invocation. Not populated with old
        # warnings.
        self.instance_warnings = WarningsDatabase()

        def on_warning(warning):
            filename = warning['filename']

            if not os.path.exists(filename):
                raise Exception('Could not find file containing warning: %s' %
                                filename)

            self.warnings_database.insert(warning)
            # Make a copy so mutations don't impact other database.
            self.instance_warnings.insert(warning.copy())

        self._warnings_collector = WarningsCollector(on_warning,
                                                     objdir=self.topobjdir)

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

    def stop_resource_recording(self):
        if self._resources_started:
            self.resources.stop()

        self._resources_started = False

    def finish(self, record_usage=True):
        """Record the end of the build."""
        self.stop_resource_recording()
        self.end_time = time.time()
        self._finder_end_cpu = self._get_finder_cpu_usage()
        self.elapsed = self.end_time - self.start_time

        self.warnings_database.prune()
        self.warnings_database.save_to_file(self._warnings_path)

        if not record_usage:
            return

        try:
            usage = self.get_resource_usage()
            if not usage:
                return

            self.log_resource_usage(usage)
            with open(self._get_state_filename('build_resources.json'), 'w') as fh:
                json.dump(self.resources.as_dict(), fh, indent=2)
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

    def get_resource_usage(self):
        """ Produce a data structure containing the low-level resource usage information.

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

        o = dict(
            version=3,
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


        # If the imports for this file ran before the in-tree virtualenv
        # was bootstrapped (for instance, for a clobber build in automation),
        # psutil might not be available.
        #
        # Treat psutil as optional to avoid an outright failure to log resources
        # TODO: it would be nice to collect data on the storage device as well
        # in this case.
        o['system'] = {}
        if psutil:
            o['system'].update(dict(
                logical_cpu_count=psutil.cpu_count(),
                physical_cpu_count=psutil.cpu_count(logical=False),
                swap_total=psutil.swap_memory()[0],
                vmem_total=psutil.virtual_memory()[0],
            ))

        return o

    def log_resource_usage(self, usage):
        """Summarize the resource usage of this build in a log message."""

        if not usage:
            return

        params = dict(
            duration=self.end_time - self.start_time,
            cpu_percent=usage['cpu_percent'],
            io_read_bytes=usage['io'].read_bytes,
            io_write_bytes=usage['io'].write_bytes,
            io_read_time=usage['io'].read_time,
            io_write_time=usage['io'].write_time,
        )

        message = 'Overall system resources - Wall time: {duration:.0f}s; ' \
            'CPU: {cpu_percent:.0f}%; ' \
            'Read bytes: {io_read_bytes}; Write bytes: {io_write_bytes}; ' \
            'Read time: {io_read_time}; Write time: {io_write_time}'

        self.log(logging.WARNING, 'resource_usage', params, message)

        excessive, sin, sout = self.have_excessive_swapping()
        if excessive is not None and (sin or sout):
            sin /= 1048576
            sout /= 1048576
            self.log(logging.WARNING, 'swap_activity',
                {'sin': sin, 'sout': sout},
                'Swap in/out (MB): {sin}/{sout}')

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


class TerminalLoggingHandler(logging.Handler):
    """Custom logging handler that works with terminal window dressing.

    This class should probably live elsewhere, like the mach core. Consider
    this a proving ground for its usefulness.
    """
    def __init__(self):
        logging.Handler.__init__(self)

        self.fh = sys.stdout
        self.footer = None

    def flush(self):
        self.acquire()

        try:
            self.fh.flush()
        finally:
            self.release()

    def emit(self, record):
        msg = self.format(record)

        self.acquire()

        try:
            if self.footer:
                self.footer.clear()

            self.fh.write(msg)
            self.fh.write('\n')

            if self.footer:
                self.footer.draw()

            # If we don't flush, the footer may not get drawn.
            self.fh.flush()
        finally:
            self.release()


class BuildProgressFooter(Footer):
    """Handles display of a build progress indicator in a terminal.

    When mach builds inside a blessings-supported terminal, it will render
    progress information collected from a BuildMonitor. This class converts the
    state of BuildMonitor into terminal output.
    """

    def __init__(self, terminal, monitor):
        Footer.__init__(self, terminal)
        self.tiers = monitor.tiers.tier_status.viewitems()

    def draw(self):
        """Draws this footer in the terminal."""

        if not self.tiers:
            return

        # The drawn terminal looks something like:
        # TIER: static export libs tools

        parts = [('bold', 'TIER:')]
        append = parts.append
        for tier, status in self.tiers:
            if status is None:
                append(tier)
            elif status == 'finished':
                append(('green', tier))
            else:
                append(('underline_yellow', tier))

        self.write(parts)


class OutputManager(LoggingMixin):
    """Handles writing job output to a terminal or log."""

    def __init__(self, log_manager, footer):
        self.populate_logger()

        self.footer = None
        terminal = log_manager.terminal

        # TODO convert terminal footer to config file setting.
        if not terminal or os.environ.get('MACH_NO_TERMINAL_FOOTER', None):
            return
        if os.environ.get('INSIDE_EMACS', None):
            return

        self.t = terminal
        self.footer = footer

        self._handler = TerminalLoggingHandler()
        self._handler.setFormatter(log_manager.terminal_formatter)
        self._handler.footer = self.footer

        old = log_manager.replace_terminal_handler(self._handler)
        self._handler.level = old.level

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self.footer:
            self.footer.clear()
            # Prevents the footer from being redrawn if logging occurs.
            self._handler.footer = None

    def write_line(self, line):
        if self.footer:
            self.footer.clear()

        print(line)

        if self.footer:
            self.footer.draw()

    def refresh(self):
        if not self.footer:
            return

        self.footer.clear()
        self.footer.draw()


class BuildOutputManager(OutputManager):
    """Handles writing build output to a terminal, to logs, etc."""

    def __init__(self, log_manager, monitor, footer):
        self.monitor = monitor
        OutputManager.__init__(self, log_manager, footer)

    def __exit__(self, exc_type, exc_value, traceback):
        OutputManager.__exit__(self, exc_type, exc_value, traceback)

        # Ensure the resource monitor is stopped because leaving it running
        # could result in the process hanging on exit because the resource
        # collection child process hasn't been told to stop.
        self.monitor.stop_resource_recording()


    def on_line(self, line):
        warning, state_changed, relevant = self.monitor.on_line(line)

        if relevant:
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')
        elif state_changed:
            have_handler = hasattr(self, 'handler')
            if have_handler:
                self.handler.acquire()
            try:
                self.refresh()
            finally:
                if have_handler:
                    self.handler.release()


class StaticAnalysisFooter(Footer):
    """Handles display of a static analysis progress indicator in a terminal.
    """

    def __init__(self, terminal, monitor):
        Footer.__init__(self, terminal)
        self.monitor = monitor

    def draw(self):
        """Draws this footer in the terminal."""

        monitor = self.monitor
        total = monitor.num_files
        processed = monitor.num_files_processed
        percent = '(%.2f%%)' % (processed * 100.0 / total)
        parts = [
            ('dim', 'Processing'),
            ('yellow', str(processed)),
            ('dim', 'of'),
            ('yellow', str(total)),
            ('dim', 'files'),
            ('green', percent)
        ]
        if monitor.current_file:
            parts.append(('bold', monitor.current_file))

        self.write(parts)


class StaticAnalysisOutputManager(OutputManager):
    """Handles writing static analysis output to a terminal."""

    def __init__(self, log_manager, monitor, footer):
        self.monitor = monitor
        OutputManager.__init__(self, log_manager, footer)

    def on_line(self, line):
        warning, relevant = self.monitor.on_line(line)

        if warning:
            self.log(logging.INFO, 'compiler_warning', warning,
                'Warning: {flag} in {filename}: {message}')

        if relevant:
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')
        else:
            have_handler = hasattr(self, 'handler')
            if have_handler:
                self.handler.acquire()
            try:
                self.refresh()
            finally:
                if have_handler:
                    self.handler.release()


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
        ('cache_hit_rate', 'cache hit rate'),
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
        ('unsupported_code_directive', 'unsupported code directive'),
        ('unsupported_compiler_option', 'unsupported compiler option'),
        ('out_stdout', 'output to stdout'),
        ('out_device', 'output to a non-regular file'),
        ('no_input', 'no input file'),
        ('bad_extra_file', 'error hashing extra file'),
        ('num_cleanups', 'cleanups performed'),
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

    def build(self, what=None, disable_extra_make_dependencies=None, jobs=0,
              directory=None, verbose=False, keep_going=False, mach_context=None):
        """Invoke the build backend.

        ``what`` defines the thing to build. If not defined, the default
        target is used.
        """
        warnings_path = self._get_state_filename('warnings.json')
        monitor = self._spawn(BuildMonitor)
        monitor.init(warnings_path)
        ccache_start = monitor.ccache_stats()
        footer = BuildProgressFooter(self.log_manager.terminal, monitor)

        # Disable indexing in objdir because it is not necessary and can slow
        # down builds.
        mkdir(self.topobjdir, not_indexed=True)

        with BuildOutputManager(self.log_manager, monitor, footer) as output:
            monitor.start()

            if directory is not None and not what:
                print('Can only use -C/--directory with an explicit target '
                    'name.')
                return 1

            if directory is not None:
                disable_extra_make_dependencies=True
                directory = mozpath.normsep(directory)
                if directory.startswith('/'):
                    directory = directory[1:]

            status = None
            monitor.start_resource_recording()
            if what:
                top_make = os.path.join(self.topobjdir, 'Makefile')
                if not os.path.exists(top_make):
                    print('Your tree has not been configured yet. Please run '
                        '|mach build| with no arguments.')
                    return 1

                # Collect target pairs.
                target_pairs = []
                for target in what:
                    path_arg = self._wrap_path_argument(target)

                    if directory is not None:
                        make_dir = os.path.join(self.topobjdir, directory)
                        make_target = target
                    else:
                        make_dir, make_target = \
                            resolve_target_to_make(self.topobjdir,
                                path_arg.relpath())

                    if make_dir is None and make_target is None:
                        return 1

                    # See bug 886162 - we don't want to "accidentally" build
                    # the entire tree (if that's really the intent, it's
                    # unlikely they would have specified a directory.)
                    if not make_dir and not make_target:
                        print("The specified directory doesn't contain a "
                              "Makefile and the first parent with one is the "
                              "root of the tree. Please specify a directory "
                              "with a Makefile or run |mach build| if you "
                              "want to build the entire tree.")
                        return 1

                    target_pairs.append((make_dir, make_target))

                # Possibly add extra make depencies using dumbmake.
                if not disable_extra_make_dependencies:
                    from dumbmake.dumbmake import (dependency_map,
                                                   add_extra_dependencies)
                    depfile = os.path.join(self.topsrcdir, 'build',
                                           'dumbmake-dependencies')
                    with open(depfile) as f:
                        dm = dependency_map(f.readlines())
                    new_pairs = list(add_extra_dependencies(target_pairs, dm))
                    self.log(logging.DEBUG, 'dumbmake',
                             {'target_pairs': target_pairs,
                              'new_pairs': new_pairs},
                             'Added extra dependencies: will build {new_pairs} ' +
                             'instead of {target_pairs}.')
                    target_pairs = new_pairs

                # Ensure build backend is up to date. The alternative is to
                # have rules in the invoked Makefile to rebuild the build
                # backend. But that involves make reinvoking itself and there
                # are undesired side-effects of this. See bug 877308 for a
                # comprehensive history lesson.
                self._run_make(directory=self.topobjdir, target='backend',
                    line_handler=output.on_line, log=False,
                    print_directory=False, keep_going=keep_going)

                # Build target pairs.
                for make_dir, make_target in target_pairs:
                    # We don't display build status messages during partial
                    # tree builds because they aren't reliable there. This
                    # could potentially be fixed if the build monitor were more
                    # intelligent about encountering undefined state.
                    status = self._run_make(directory=make_dir, target=make_target,
                        line_handler=output.on_line, log=False, print_directory=False,
                        ensure_exit_code=False, num_jobs=jobs, silent=not verbose,
                        append_env={b'NO_BUILDSTATUS_MESSAGES': b'1'},
                        keep_going=keep_going)

                    if status != 0:
                        break
            else:
                # Try to call the default backend's build() method. This will
                # run configure to determine BUILD_BACKENDS if it hasn't run
                # yet.
                config = None
                try:
                    config = self.config_environment
                except Exception:
                    config_rc = self.configure(buildstatus_messages=True,
                                               line_handler=output.on_line)
                    if config_rc != 0:
                        return config_rc

                    # Even if configure runs successfully, we may have trouble
                    # getting the config_environment for some builds, such as
                    # OSX Universal builds. These have to go through client.mk
                    # regardless.
                    try:
                        config = self.config_environment
                    except Exception:
                        pass

                if config:
                    active_backend = config.substs.get('BUILD_BACKENDS', [None])[0]
                    if active_backend:
                        backend_cls = get_backend_class(active_backend)(config)
                        status = backend_cls.build(self, output, jobs, verbose)

                # If the backend doesn't specify a build() method, then just
                # call client.mk directly.
                if status is None:
                    status = self._run_client_mk(line_handler=output.on_line,
                                                 jobs=jobs,
                                                 verbose=verbose,
                                                 keep_going=keep_going)

                self.log(logging.WARNING, 'warning_summary',
                    {'count': len(monitor.warnings_database)},
                    '{count} compiler warnings present.')

            monitor.finish(record_usage=status == 0)

        # Print the collected compiler warnings. This is redundant with
        # inline output from the compiler itself. However, unlike inline
        # output, this list is sorted and grouped by file, making it
        # easier to triage output.
        #
        # Only do this if we had a successful build. If the build failed,
        # there are more important things in the log to look for than
        # whatever code we warned about.
        if not status:
            # Suppress warnings for 3rd party projects in local builds
            # until we suppress them for real.
            # TODO remove entries/feature once we stop generating warnings
            # in these directories.
            pathToThirdparty = os.path.join(self.topsrcdir,
                                            "tools",
                                           "rewriting",
                                           "ThirdPartyPaths.txt")

            if os.path.exists(pathToThirdparty):
                with open(pathToThirdparty) as f:
                    # Normalize the path (no trailing /)
                    LOCAL_SUPPRESS_DIRS = tuple(d.rstrip('/') for d in f.read().splitlines())
            else:
                # For application based on gecko like thunderbird
                LOCAL_SUPPRESS_DIRS = ()

            suppressed_by_dir = Counter()

            for warning in sorted(monitor.instance_warnings):
                path = mozpath.normsep(warning['filename'])
                if path.startswith(self.topsrcdir):
                    path = path[len(self.topsrcdir) + 1:]

                warning['normpath'] = path

                if (path.startswith(LOCAL_SUPPRESS_DIRS) and
                        'MOZ_AUTOMATION' not in os.environ):
                    for d in LOCAL_SUPPRESS_DIRS:
                        if path.startswith(d):
                            suppressed_by_dir[d] += 1
                            break

                    continue

                if warning['column'] is not None:
                    self.log(logging.WARNING, 'compiler_warning', warning,
                             'warning: {normpath}:{line}:{column} [{flag}] '
                             '{message}')
                else:
                    self.log(logging.WARNING, 'compiler_warning', warning,
                             'warning: {normpath}:{line} [{flag}] {message}')

            for d, count in sorted(suppressed_by_dir.items()):
                self.log(logging.WARNING, 'suppressed_warning',
                         {'dir': d, 'count': count},
                         '(suppressed {count} warnings in {dir})')

        high_finder, finder_percent = monitor.have_high_finder_usage()
        if high_finder:
            print(FINDER_SLOW_MESSAGE % finder_percent)

        ccache_end = monitor.ccache_stats()

        ccache_diff = None
        if ccache_start and ccache_end:
            ccache_diff = ccache_end - ccache_start
            if ccache_diff:
                self.log(logging.INFO, 'ccache',
                         {'msg': ccache_diff.hit_rate_message()}, "{msg}")

        notify_minimum_time = 300
        try:
            notify_minimum_time = int(os.environ.get('MACH_NOTIFY_MINTIME', '300'))
        except ValueError:
            # Just stick with the default
            pass

        if monitor.elapsed > notify_minimum_time:
            # Display a notification when the build completes.
            self.notify('Build complete' if not status else 'Build failed')

        if status:
            return status

        long_build = monitor.elapsed > 600

        if long_build:
            output.on_line('We know it took a while, but your build finally finished successfully!')
        else:
            output.on_line('Your build was successful!')

        if monitor.have_resource_usage:
            excessive, swap_in, swap_out = monitor.have_excessive_swapping()
            # if excessive:
            #    print(EXCESSIVE_SWAP_MESSAGE)

            print('To view resource usage of the build, run |mach '
                'resource-usage|.')

            telemetry_handler = getattr(mach_context,
                                        'telemetry_handler', None)
            telemetry_data = monitor.get_resource_usage()

            # Record build configuration data. For now, we cherry pick
            # items we need rather than grabbing everything, in order
            # to avoid accidentally disclosing PII.
            telemetry_data['substs'] = {}
            try:
                for key in ['MOZ_ARTIFACT_BUILDS', 'MOZ_USING_CCACHE', 'MOZ_USING_SCCACHE']:
                    value = self.substs.get(key, False)
                    telemetry_data['substs'][key] = value
            except BuildEnvironmentNotFoundException:
                pass

            # Grab ccache stats if available. We need to be careful not
            # to capture information that can potentially identify the
            # user (such as the cache location)
            if ccache_diff:
                telemetry_data['ccache'] = {}
                for key in [key[0] for key in ccache_diff.STATS_KEYS]:
                    try:
                        telemetry_data['ccache'][key] = ccache_diff._values[key]
                    except KeyError:
                        pass

            if telemetry_handler:
                telemetry_handler(mach_context, telemetry_data)

        # Only for full builds because incremental builders likely don't
        # need to be burdened with this.
        if not what:
            try:
                # Fennec doesn't have useful output from just building. We should
                # arguably make the build action useful for Fennec. Another day...
                if self.substs['MOZ_BUILD_APP'] != 'mobile/android':
                    print('To take your build for a test drive, run: |mach run|')
                app = self.substs['MOZ_BUILD_APP']
                if app in ('browser', 'mobile/android'):
                    print('For more information on what to do now, see '
                        'https://developer.mozilla.org/docs/Developer_Guide/So_You_Just_Built_Firefox')
            except Exception:
                # Ignore Exceptions in case we can't find config.status (such
                # as when doing OSX Universal builds)
                pass

        return status

    def configure(self, options=None, buildstatus_messages=False,
                  line_handler=None):
        # Disable indexing in objdir because it is not necessary and can slow
        # down builds.
        mkdir(self.topobjdir, not_indexed=True)

        def on_line(line):
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')

        line_handler = line_handler or on_line

        options = ' '.join(shell_quote(o) for o in options or ())
        append_env = {b'CONFIGURE_ARGS': options.encode('utf-8')}

        # Only print build status messages when we have an active
        # monitor.
        if not buildstatus_messages:
            append_env[b'NO_BUILDSTATUS_MESSAGES'] =  b'1'
        status = self._run_client_mk(target='configure',
                                     line_handler=line_handler,
                                     append_env=append_env)

        if not status:
            print('Configure complete!')
            print('Be sure to run |mach build| to pick up any changes');

        return status

    def install_tests(self, test_objs):
        """Install test files."""

        if self.is_clobber_needed():
            print(INSTALL_TESTS_CLOBBER.format(
                  clobber_file=os.path.join(self.topobjdir, 'CLOBBER')))
            sys.exit(1)

        if not test_objs:
            # If we don't actually have a list of tests to install we install
            # test and support files wholesale.
            self._run_make(target='install-test-files', pass_thru=True,
                           print_directory=False)
        else:
            install_test_files(mozpath.normpath(self.topsrcdir), self.topobjdir,
                               '_tests', test_objs)

    def _run_client_mk(self, target=None, line_handler=None, jobs=0,
                       verbose=None, keep_going=False, append_env=None):
        append_env = dict(append_env or {})
        append_env['TOPSRCDIR'] = self.topsrcdir

        append_env['CONFIG_GUESS'] = self.resolve_config_guess()

        mozconfig = self.mozconfig

        if self._check_clobber(mozconfig, os.environ):
            return 1

        mozconfig_make_lines = []
        for arg in mozconfig['make_extra'] or []:
            mozconfig_make_lines.append(arg)

        if mozconfig['make_flags']:
            mozconfig_make_lines.append(b'MOZ_MAKE_FLAGS=%s' %
                                        b' '.join(mozconfig['make_flags']))
        objdir = mozpath.normsep(self.topobjdir)
        mozconfig_make_lines.append(b'MOZ_OBJDIR=%s' % objdir)
        mozconfig_make_lines.append(b'OBJDIR=%s' % objdir)

        if mozconfig['path']:
            mozconfig_make_lines.append(b'FOUND_MOZCONFIG=%s' %
                                        mozpath.normsep(mozconfig['path']))
            mozconfig_make_lines.append(b'export FOUND_MOZCONFIG')

        # The .mozconfig.mk file only contains exported variables and lines with
        # UPLOAD_EXTRA_FILES.
        mozconfig_filtered_lines = [
            line for line in mozconfig_make_lines
            # Bug 1418122 investigate why UPLOAD_EXTRA_FILES is special and
            # remove it.
            if line.startswith(b'export ') or b'UPLOAD_EXTRA_FILES' in line
        ]

        mozconfig_client_mk = os.path.join(self.topobjdir,
                                           '.mozconfig-client-mk')
        with FileAvoidWrite(mozconfig_client_mk) as fh:
            fh.write(b'\n'.join(mozconfig_make_lines))

        mozconfig_mk = os.path.join(self.topobjdir, '.mozconfig.mk')
        with FileAvoidWrite(mozconfig_mk) as fh:
            fh.write(b'\n'.join(mozconfig_filtered_lines))

        mozconfig_json = os.path.join(self.topobjdir, '.mozconfig.json')
        with FileAvoidWrite(mozconfig_json) as fh:
            json.dump({
                'topsrcdir': self.topsrcdir,
                'topobjdir': self.topobjdir,
                'mozconfig': mozconfig,
            }, fh, sort_keys=True, indent=2)

        # Copy the original mozconfig to the objdir.
        mozconfig_objdir = os.path.join(self.topobjdir, '.mozconfig')
        if mozconfig['path']:
            with open(mozconfig['path'], 'rb') as ifh:
                with FileAvoidWrite(mozconfig_objdir) as ofh:
                    ofh.write(ifh.read())
        else:
            try:
                os.unlink(mozconfig_objdir)
            except OSError as e:
                if e.errno != errno.ENOENT:
                    raise

        if mozconfig_make_lines:
            self.log(logging.WARNING, 'mozconfig_content', {
                'path': mozconfig['path'],
                'content': '\n    '.join(mozconfig_make_lines),
            }, 'Adding make options from {path}\n    {content}')

        append_env['OBJDIR'] = mozpath.normsep(self.topobjdir)

        return self._run_make(srcdir=True,
                              filename='client.mk',
                              allow_parallel=False,
                              ensure_exit_code=False,
                              print_directory=False,
                              target=target,
                              line_handler=line_handler,
                              log=False,
                              num_jobs=jobs,
                              silent=not verbose,
                              keep_going=keep_going,
                              append_env=append_env)

    def _check_clobber(self, mozconfig, env):
        auto_clobber = any([
            env.get('AUTOCLOBBER', False),
            (mozconfig['env'] or {}).get('added', {}).get('AUTOCLOBBER', False),
            'AUTOCLOBBER=1' in (mozconfig['make_extra'] or []),
        ])

        clobberer = Clobberer(self.topsrcdir, self.topobjdir)
        clobber_output = io.BytesIO()
        res = clobberer.maybe_do_clobber(os.getcwd(), auto_clobber,
                                         clobber_output)
        clobber_output.seek(0)
        for line in clobber_output.readlines():
            self.log(logging.WARNING, 'clobber',
                     {'msg': line.rstrip()}, '{msg}')

        clobber_required, clobber_performed, clobber_message = res
        if not clobber_required or clobber_performed:
            if clobber_performed and env.get('TINDERBOX_OUTPUT'):
                self.log(logging.WARNING, 'clobber',
                         {'msg': 'TinderboxPrint: auto clobber'}, '{msg}')
        else:
            for line in clobber_message.splitlines():
                self.log(logging.WARNING, 'clobber',
                         {'msg': line.rstrip()}, '{msg}')
            return True

        return False
