# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import getpass
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

    def __init__(self):
        self.tiers = OrderedDict()
        self.active_tier = None
        self.active_subtiers = set()

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
        t['duration'] = t['finish_time'] - t['begin_time']

    def begin_subtier(self, tier, subtier, dirs):
        """Record that execution of a subtier has begun."""
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

        st['duration'] = st['finish_time'] - st['begin_time']

    def begin_dir(self, tier, subtier, d):
        """Record that execution of a directory has begun."""
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
        entry['duration'] = entry['finish_time'] - entry['begin_time']

        if self.active_dirs[subtier]:
            entry['concurrent'] = True

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


class BuildMonitor(MozbuildObject):
    """Monitors the output of the build."""

    def init(self, warnings_path):
        """Create a new monitor.

        warnings_path is a path of a warnings database to use.
        """
        self._warnings_path = warnings_path

        self.tiers = TierStatus()

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

    def finish(self):
        """Record the end of the build."""
        self.end_time = time.time()
        self._finder_end_cpu = self._get_finder_cpu_usage()
        self.elapsed = self.end_time - self.start_time

        self.warnings_database.prune()
        self.warnings_database.save_to_file(self._warnings_path)

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
