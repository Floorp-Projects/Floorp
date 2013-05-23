# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import getpass
import os
import sys
import time

from collections import namedtuple

try:
    import psutil
except ImportError:
    psutil = None

from ..compilation.warnings import (
    WarningsCollector,
    WarningsDatabase,
)


BuildOutputResult = namedtuple('BuildOutputResult',
    ('warning', 'state_changed', 'for_display'))


class BuildMonitor(object):
    """Monitors the output of the build."""

    def __init__(self, topobjdir, warnings_path):
        """Create a new monitor.

        warnings_path is a path of a warnings database to use.
        """
        self._warnings_path = warnings_path

        self.tiers = []
        self.subtiers = []
        self.current_tier = None
        self.current_subtier = None
        self.current_tier_dirs = []
        self.current_tier_static_dirs = []
        self.current_subtier_dirs = []
        self.current_tier_dir = None
        self.current_tier_dir_index = 0

        self.warnings_database = WarningsDatabase()
        if os.path.exists(warnings_path):
            try:
                self.warnings_database.load_from_file(warnings_path)
            except ValueError:
                os.remove(warnings_path)

        self._warnings_collector = WarningsCollector(
            database=self.warnings_database, objdir=topobjdir)

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
                self.tiers = args
                update_needed = False
            elif action == 'SUBTIERS':
                self.subtiers = args
                update_needed = False
            elif action == 'STATICDIRS':
                self.current_tier_static_dirs = args
                update_needed = False
            elif action == 'DIRS':
                self.current_tier_dirs = args
                update_needed = False
            elif action == 'TIER_START':
                assert len(args) == 1
                self.current_tier = args[0]
                self.current_subtier = None
                self.current_tier_dirs = []
                self.current_tier_dir = None
            elif action == 'TIER_FINISH':
                assert len(args) == 1
                assert args[0] == self.current_tier
            elif action == 'SUBTIER_START':
                assert len(args) == 2
                tier, subtier = args
                assert tier == self.current_tier
                self.current_subtier = subtier
                if subtier == 'static':
                    self.current_subtier_dirs = self.current_tier_static_dirs
                else:
                    self.current_subtier_dirs = self.current_tier_dirs
                self.current_tier_dir_index = 0
            elif action == 'SUBTIER_FINISH':
                assert len(args) == 2
                tier, subtier = args
                assert tier == self.current_tier
                assert subtier == self.current_subtier
            elif action == 'TIERDIR_START':
                assert len(args) == 1
                self.current_tier_dir = args[0]
                self.current_tier_dir_index += 1
            elif action == 'TIERDIR_FINISH':
                assert len(args) == 1
                assert self.current_tier_dir == args[0]
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
