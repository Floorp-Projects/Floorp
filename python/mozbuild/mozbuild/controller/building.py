# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import getpass
import json
import logging
import os
import sys
import time

from collections import (
    namedtuple,
    OrderedDict,
)

# keep in sync with psutil os support, see psutil/__init__.py
if sys.platform.startswith("freebsd") or sys.platform.startswith("darwin") or sys.platform.startswith("win32") or sys.platform.startswith("linux"):
    try:
        import psutil
    except ImportError:
        psutil = None
else:
    psutil = None

from mozsystemmonitor.resourcemonitor import SystemResourceMonitor

from ..base import MozbuildObject

from ..compilation.warnings import (
    WarningsCollector,
    WarningsDatabase,
)


BuildOutputResult = namedtuple('BuildOutputResult',
    ('warning', 'state_changed', 'for_display'))


class TierStatus(object):
    """Represents the state and progress of tier traversal.

    The build system is organized into linear phases called tiers. Each tier
    executes in the order it was defined, 1 at a time.

    Tiers can have subtiers. Subtiers can execute in any order. Some subtiers
    execute sequentially. Others are concurrent.

    Subtiers can have directories. Directories can execute in any order, just
    like subtiers.
    """

    def __init__(self, resources):
        """Accepts a SystemResourceMonitor to record results against."""
        self.tiers = OrderedDict()
        self.active_tier = None
        self.active_subtiers = set()
        self.resources = resources

    def set_tiers(self, tiers):
        """Record the set of known tiers."""
        for tier in tiers:
            self.tiers[tier] = dict(
                begin_time=None,
                finish_time=None,
                duration=None,
                subtiers=OrderedDict(),
            )

    def begin_tier(self, tier, subtiers):
        """Record that execution of a tier has begun."""
        t = self.tiers[tier]
        # We should ideally use a monotonic clock here. Unfortunately, we won't
        # have one until Python 3.
        t['begin_time'] = time.time()
        self.resources.begin_phase(self._phase(tier))
        for subtier in subtiers:
            t['subtiers'][subtier] = dict(
                begin_time=None,
                finish_time=None,
                duration=None,
                concurrent=False,
                dirs=OrderedDict(),
                dirs_complete=0,
            )

        self.active_tier = tier
        self.active_subtiers = set()
        self.active_dirs = {}

    def finish_tier(self, tier):
        """Record that execution of a tier has finished."""
        t = self.tiers[tier]
        t['finish_time'] = time.time()
        t['duration'] = self.resources.finish_phase(self._phase(tier))

    def begin_subtier(self, tier, subtier, dirs):
        """Record that execution of a subtier has begun."""
        self.resources.begin_phase(self._phase(tier, subtier))
        st = self.tiers[tier]['subtiers'][subtier]
        st['begin_time'] = time.time()

        for d in dirs:
            st['dirs'][d] = dict(
                begin_time=None,
                finish_time=None,
                duration=None,
                concurrent=False,
            )

        if self.active_subtiers:
            st['concurrent'] = True

        self.active_subtiers.add(subtier)

    def finish_subtier(self, tier, subtier):
        """Record that execution of a subtier has finished."""
        st = self.tiers[tier]['subtiers'][subtier]
        st['finish_time'] = time.time()

        self.active_subtiers.remove(subtier)
        if self.active_subtiers:
            st['concurrent'] = True

        # A subtier may not have directories.
        try:
            del self.active_dirs[subtier]
        except KeyError:
            pass

        st['duration'] = self.resources.finish_phase(self._phase(tier, subtier))

    def begin_dir(self, tier, subtier, d):
        """Record that execution of a directory has begun."""
        self.resources.begin_phase(self._phase(tier, subtier, d))
        entry = self.tiers[tier]['subtiers'][subtier]['dirs'][d]
        entry['begin_time'] = time.time()

        self.active_dirs.setdefault(subtier, set()).add(d)

        if len(self.active_dirs[subtier]) > 1:
            entry['concurrent'] = True

    def finish_dir(self, tier, subtier, d):
        """Record that execution of a directory has finished."""
        st = self.tiers[tier]['subtiers'][subtier]
        st['dirs_complete'] += 1

        entry = st['dirs'][d]
        entry['finish_time'] = time.time()

        self.active_dirs[subtier].remove(d)
        if self.active_dirs[subtier]:
            entry['concurrent'] = True

        entry['duration'] = self.resources.finish_phase(self._phase(tier,
            subtier, d))

    def tier_status(self):
        for tier, state in self.tiers.items():
            active = self.active_tier == tier
            finished = state['finish_time'] is not None

            yield tier, active, finished

    def current_subtier_status(self):
        for subtier, state in self.tiers[self.active_tier]['subtiers'].items():
            active = subtier in self.active_subtiers
            finished = state['finish_time'] is not None

            yield subtier, active, finished

    def current_dirs_status(self):
        for subtier, dirs in self.active_dirs.items():
            st = self.tiers[self.active_tier]['subtiers'][subtier]
            yield subtier, st['dirs'].keys(), dirs, st['dirs_complete']

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
                subtiers=[],
            )

            self.add_resources_to_dict(t_entry, phase=self._phase(tier))

            for subtier, state in state['subtiers'].items():
                st_entry = dict(
                    name=subtier,
                    start=state['begin_time'],
                    end=state['finish_time'],
                    duration=state['duration'],
                    concurrent=state['concurrent'],
                    dirs=[],
                )

                self.add_resources_to_dict(st_entry, phase=self._phase(tier,
                    subtier))

                for d, state in state['dirs'].items():
                    d_entry = dict(
                        name=d,
                        start=state['begin_time'],
                        end=state['finish_time'],
                        duration=state['duration'],
                    )

                    self.add_resources_to_dict(d_entry, phase=self._phase(tier,
                        subtier, d))

                    st_entry['dirs'].append(d_entry)

                t_entry['subtiers'].append(st_entry)

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

    def _phase(self, tier, subtier=None, d=None):
        parts = [tier]
        if subtier:
            parts.append(subtier)
        if d:
            parts.append(d)

        return '_'.join(parts)


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
                subtiers = args[1:]
                self.tiers.begin_tier(tier, subtiers)
            elif action == 'TIER_FINISH':
                tier, = args
                self.tiers.finish_tier(tier)
            elif action == 'SUBTIER_START':
                tier, subtier = args[0:2]
                dirs = args[2:]
                self.tiers.begin_subtier(tier, subtier, dirs)
            elif action == 'SUBTIER_FINISH':
                tier, subtier = args
                self.tiers.finish_subtier(tier, subtier)
            elif action == 'TIERDIR_START':
                tier, subtier, d = args
                self.tiers.begin_dir(tier, subtier, d)
            elif action == 'TIERDIR_FINISH':
                tier, subtier, d = args
                self.tiers.finish_dir(tier, subtier, d)
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

        usage = self.record_resource_usage()
        if not usage:
            return

        with open(self._get_state_filename('build_resources.json'), 'w') as fh:
            json.dump(usage, fh, indent=2)

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

        o = dict(
            version=1,
            start=self.start_time,
            end=self.end_time,
            duration=self.end_time - self.start_time,
            resources=[],
            cpu_percent=cpu_percent,
            cpu_times=cpu_times,
            io=io,
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
