# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import errno
import getpass
import io
import json
import logging
import os
import re
import shutil
import six
import subprocess
import sys
import time

from collections import (
    Counter,
    namedtuple,
    OrderedDict,
)
from textwrap import TextWrapper

from mach.site import CommandSiteManager

try:
    import psutil
except Exception:
    psutil = None

from mach.mixin.logging import LoggingMixin
from mach.util import get_state_dir
import mozfile
from mozsystemmonitor.resourcemonitor import SystemResourceMonitor
from mozterm.widgets import Footer

import mozpack.path as mozpath

from .clobber import Clobberer
from ..base import MozbuildObject
from ..backend import get_backend_class
from ..testing import install_test_files
from ..compilation.warnings import (
    WarningsCollector,
    WarningsDatabase,
)
from ..util import (
    FileAvoidWrite,
    mkdir,
    resolve_target_to_make,
)


FINDER_SLOW_MESSAGE = """
===================
PERFORMANCE WARNING

The OS X Finder application (file indexing used by Spotlight) used a lot of CPU
during the build - an average of %f%% (100%% is 1 core). This made your build
slower.

Consider adding ".noindex" to the end of your object directory name to have
Finder ignore it. Or, add an indexing exclusion through the Spotlight System
Preferences.
===================
""".strip()


INSTALL_TESTS_CLOBBER = "".join(
    [
        TextWrapper().fill(line) + "\n"
        for line in """
The build system was unable to install tests because the CLOBBER file has \
been updated. This means if you edited any test files, your changes may not \
be picked up until a full/clobber build is performed.

The easiest and fastest way to perform a clobber build is to run:

 $ mach clobber
 $ mach build

If you did not modify any test files, it is safe to ignore this message \
and proceed with running tests. To do this run:

 $ touch {clobber_file}
""".splitlines()
    ]
)

CLOBBER_REQUESTED_MESSAGE = """
===================
The CLOBBER file was updated prior to this build. A clobber build may be
required to succeed, but we weren't expecting it to.

Please consider filing a bug for this failure if you have reason to believe
this is a clobber bug and not due to local changes.
===================
""".strip()


BuildOutputResult = namedtuple(
    "BuildOutputResult", ("warning", "state_changed", "message")
)


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
        self.tier_status[tier] = "active"
        t = self.tiers[tier]
        # We should ideally use a monotonic clock here. Unfortunately, we won't
        # have one until Python 3.
        t["begin_time"] = time.time()
        self.resources.begin_phase(tier)

    def finish_tier(self, tier):
        """Record that execution of a tier has finished."""
        self.tier_status[tier] = "finished"
        t = self.tiers[tier]
        t["finish_time"] = time.time()
        t["duration"] = self.resources.finish_phase(tier)

    def tiered_resource_usage(self):
        """Obtains an object containing resource usage for tiers.

        The returned object is suitable for serialization.
        """
        o = []

        for tier, state in self.tiers.items():
            t_entry = dict(
                name=tier,
                start=state["begin_time"],
                end=state["finish_time"],
                duration=state["duration"],
            )

            self.add_resources_to_dict(t_entry, phase=tier)

            o.append(t_entry)

        return o

    def add_resources_to_dict(self, entry, start=None, end=None, phase=None):
        """Helper function to append resource information to a dict."""
        cpu_percent = self.resources.aggregate_cpu_percent(
            start=start, end=end, phase=phase, per_cpu=False
        )
        cpu_times = self.resources.aggregate_cpu_times(
            start=start, end=end, phase=phase, per_cpu=False
        )
        io = self.resources.aggregate_io(start=start, end=end, phase=phase)

        if cpu_percent is None:
            return entry

        entry["cpu_percent"] = cpu_percent
        entry["cpu_times"] = list(cpu_times)
        entry["io"] = list(io)

        return entry

    def add_resource_fields_to_dict(self, d):
        for usage in self.resources.range_usage():
            cpu_times = self.resources.aggregate_cpu_times(per_cpu=False)

            d["cpu_times_fields"] = list(cpu_times._fields)
            d["io_fields"] = list(usage.io._fields)
            d["virt_fields"] = list(usage.virt._fields)
            d["swap_fields"] = list(usage.swap._fields)

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
            # Skip `errors`
            if warning["type"] == "error":
                return

            filename = warning["filename"]

            if not os.path.exists(filename):
                raise Exception("Could not find file containing warning: %s" % filename)

            self.warnings_database.insert(warning)
            # Make a copy so mutations don't impact other database.
            self.instance_warnings.insert(warning.copy())

        self._warnings_collector = WarningsCollector(on_warning, objdir=self.topobjdir)
        self._build_tasks = []

        self.build_objects = []
        self.build_dirs = set()

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

        message is either None, or the content of a message to be
        displayed to the user.
        """
        message = None

        if line.startswith("BUILDSTATUS"):
            args = line.split()[1:]

            action = args.pop(0)
            update_needed = True

            if action == "TIERS":
                self.tiers.set_tiers(args)
                update_needed = False
            elif action == "TIER_START":
                tier = args[0]
                self.tiers.begin_tier(tier)
            elif action == "TIER_FINISH":
                (tier,) = args
                self.tiers.finish_tier(tier)
            elif action == "OBJECT_FILE":
                self.build_objects.append(args[0])
                update_needed = False
            elif action == "BUILD_VERBOSE":
                build_dir = args[0]
                if build_dir not in self.build_dirs:
                    self.build_dirs.add(build_dir)
                    message = build_dir
                update_needed = False
            else:
                raise Exception("Unknown build status: %s" % action)

            return BuildOutputResult(None, update_needed, message)
        elif line.startswith("BUILDTASK"):
            _, data = line.split(maxsplit=1)
            # Check that we can parse the JSON. Skip this line if we can't;
            # we'll be missing data, but that's not a huge deal.
            try:
                json.loads(data)
                self._build_tasks.append(data)
            except json.decoder.JSONDecodeError:
                pass
            return BuildOutputResult(None, False, None)

        warning = None

        try:
            warning = self._warnings_collector.process_line(line)
            message = line
        except Exception:
            pass

        return BuildOutputResult(warning, False, message)

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

        if "MOZ_AUTOMATION" not in os.environ:
            build_tasks_path = self._get_state_filename("build_tasks.json")
            with io.open(build_tasks_path, "w", encoding="utf-8", newline="\n") as fh:
                fh.write("[")
                first = True
                for task in self._build_tasks:
                    # We've already verified all of these are valid JSON, so we
                    # can write the data out to the file directly.
                    fh.write("%s\n  %s" % ("," if not first else "", task))
                    first = False
                fh.write("\n]\n")

        # Record usage.
        if not record_usage:
            return

        try:
            usage = self.get_resource_usage()
            if not usage:
                return

            self.log_resource_usage(usage)
            # When running on automation, we store the resource usage data in
            # the upload path, alongside, for convenience, a copy of the HTML
            # viewer.
            if "MOZ_AUTOMATION" in os.environ and "UPLOAD_PATH" in os.environ:
                build_resources_path = os.path.join(
                    os.environ["UPLOAD_PATH"], "build_resources.json"
                )
                shutil.copy(
                    os.path.join(
                        self.topsrcdir,
                        "python",
                        "mozbuild",
                        "mozbuild",
                        "resources",
                        "html-build-viewer",
                        "build_resources.html",
                    ),
                    os.environ["UPLOAD_PATH"],
                )
            else:
                build_resources_path = self._get_state_filename("build_resources.json")
            with io.open(
                build_resources_path, "w", encoding="utf-8", newline="\n"
            ) as fh:
                to_write = six.ensure_text(
                    json.dumps(self.resources.as_dict(), indent=2)
                )
                fh.write(to_write)
        except Exception as e:
            self.log(
                logging.WARNING,
                "build_resources_error",
                {"msg": str(e)},
                "Exception when writing resource usage file: {msg}",
            )

    def _get_finder_cpu_usage(self):
        """Obtain the CPU usage of the Finder app on OS X.

        This is used to detect high CPU usage.
        """
        if not sys.platform.startswith("darwin"):
            return None

        if not psutil:
            return None

        for proc in psutil.process_iter():
            if proc.name != "Finder":
                continue

            if proc.username != getpass.getuser():
                continue

            # Try to isolate system finder as opposed to other "Finder"
            # processes.
            if not proc.exe.endswith("CoreServices/Finder.app/Contents/MacOS/Finder"):
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
        """Produce a data structure containing the low-level resource usage information.

        This data structure can e.g. be serialized into JSON and saved for
        subsequent analysis.

        If no resource usage is available, None is returned.
        """
        if not self.have_resource_usage:
            return None

        cpu_percent = self.resources.aggregate_cpu_percent(phase=None, per_cpu=False)
        cpu_times = self.resources.aggregate_cpu_times(phase=None, per_cpu=False)
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
            objects=self.build_objects,
        )

        o["tiers"] = self.tiers.tiered_resource_usage()

        self.tiers.add_resource_fields_to_dict(o)

        for usage in self.resources.range_usage():
            cpu_percent = self.resources.aggregate_cpu_percent(
                usage.start, usage.end, per_cpu=False
            )
            cpu_times = self.resources.aggregate_cpu_times(
                usage.start, usage.end, per_cpu=False
            )

            entry = dict(
                start=usage.start,
                end=usage.end,
                virt=list(usage.virt),
                swap=list(usage.swap),
            )

            self.tiers.add_resources_to_dict(entry, start=usage.start, end=usage.end)

            o["resources"].append(entry)

        # If the imports for this file ran before the in-tree virtualenv
        # was bootstrapped (for instance, for a clobber build in automation),
        # psutil might not be available.
        #
        # Treat psutil as optional to avoid an outright failure to log resources
        # TODO: it would be nice to collect data on the storage device as well
        # in this case.
        o["system"] = {}
        if psutil:
            o["system"].update(
                dict(
                    logical_cpu_count=psutil.cpu_count(),
                    physical_cpu_count=psutil.cpu_count(logical=False),
                    swap_total=psutil.swap_memory()[0],
                    vmem_total=psutil.virtual_memory()[0],
                )
            )

        return o

    def log_resource_usage(self, usage):
        """Summarize the resource usage of this build in a log message."""

        if not usage:
            return

        params = dict(
            duration=self.end_time - self.start_time,
            cpu_percent=usage["cpu_percent"],
            io_read_bytes=usage["io"].read_bytes,
            io_write_bytes=usage["io"].write_bytes,
            io_read_time=usage["io"].read_time,
            io_write_time=usage["io"].write_time,
        )

        message = (
            "Overall system resources - Wall time: {duration:.0f}s; "
            "CPU: {cpu_percent:.0f}%; "
            "Read bytes: {io_read_bytes}; Write bytes: {io_write_bytes}; "
            "Read time: {io_read_time}; Write time: {io_write_time}"
        )

        self.log(logging.WARNING, "resource_usage", params, message)

        excessive, sin, sout = self.have_excessive_swapping()
        if excessive is not None and (sin or sout):
            sin /= 1048576
            sout /= 1048576
            self.log(
                logging.WARNING,
                "swap_activity",
                {"sin": sin, "sout": sout},
                "Swap in/out (MB): {sin}/{sout}",
            )

    def ccache_stats(self):
        ccache_stats = None

        ccache = mozfile.which("ccache")
        if ccache:
            # With CCache v4.4+ statistics might require --verbose
            is_version_4_4_or_newer = CCacheStats.check_version_4_4_or_newer(ccache)
            try:
                output = subprocess.check_output(
                    [ccache, "-s" if not is_version_4_4_or_newer else "-sv"],
                    universal_newlines=True,
                )
                ccache_stats = CCacheStats(output, is_version_4_4_or_newer)
            except ValueError as e:
                self.log(logging.WARNING, "ccache", {"msg": str(e)}, "{msg}")
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
            self.fh.write("\n")

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
        self.tiers = six.viewitems(monitor.tiers.tier_status)

    def draw(self):
        """Draws this footer in the terminal."""

        if not self.tiers:
            return

        # The drawn terminal looks something like:
        # TIER: static export libs tools

        parts = [("bold", "TIER:")]
        append = parts.append
        for tier, status in self.tiers:
            if status is None:
                append(tier)
            elif status == "finished":
                append(("green", tier))
            else:
                append(("underline_yellow", tier))

        self.write(parts)


class OutputManager(LoggingMixin):
    """Handles writing job output to a terminal or log."""

    def __init__(self, log_manager, footer):
        self.populate_logger()

        self.footer = None
        terminal = log_manager.terminal

        # TODO convert terminal footer to config file setting.
        if not terminal or os.environ.get("MACH_NO_TERMINAL_FOOTER", None):
            return
        if os.environ.get("INSIDE_EMACS", None):
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
        warning, state_changed, message = self.monitor.on_line(line)

        if message:
            self.log(logging.INFO, "build_output", {"line": message}, "{line}")
        elif state_changed:
            have_handler = hasattr(self, "handler")
            if have_handler:
                self.handler.acquire()
            try:
                self.refresh()
            finally:
                if have_handler:
                    self.handler.release()


class StaticAnalysisFooter(Footer):
    """Handles display of a static analysis progress indicator in a terminal."""

    def __init__(self, terminal, monitor):
        Footer.__init__(self, terminal)
        self.monitor = monitor

    def draw(self):
        """Draws this footer in the terminal."""

        monitor = self.monitor
        total = monitor.num_files
        processed = monitor.num_files_processed
        percent = "(%.2f%%)" % (processed * 100.0 / total)
        parts = [
            ("dim", "Processing"),
            ("yellow", str(processed)),
            ("dim", "of"),
            ("yellow", str(total)),
            ("dim", "files"),
            ("green", percent),
        ]
        if monitor.current_file:
            parts.append(("bold", monitor.current_file))

        self.write(parts)


class StaticAnalysisOutputManager(OutputManager):
    """Handles writing static analysis output to a terminal or file."""

    def __init__(self, log_manager, monitor, footer):
        self.monitor = monitor
        self.raw = ""
        OutputManager.__init__(self, log_manager, footer)

    def on_line(self, line):
        warning, relevant = self.monitor.on_line(line)
        if relevant:
            self.raw += line + "\n"

        if warning:
            self.log(
                logging.INFO,
                "compiler_warning",
                warning,
                "Warning: {flag} in {filename}: {message}",
            )

        if relevant:
            self.log(logging.INFO, "build_output", {"line": line}, "{line}")
        else:
            have_handler = hasattr(self, "handler")
            if have_handler:
                self.handler.acquire()
            try:
                self.refresh()
            finally:
                if have_handler:
                    self.handler.release()

    def write(self, path, output_format):
        assert output_format in ("text", "json"), "Invalid output format {}".format(
            output_format
        )
        path = os.path.realpath(path)

        if output_format == "json":
            self.monitor._warnings_database.save_to_file(path)

        else:
            with io.open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write(self.raw)

        self.log(
            logging.INFO,
            "write_output",
            {"path": path, "format": output_format},
            "Wrote {format} output in {path}",
        )


class CCacheStats(object):
    """Holds statistics from ccache.

    Instances can be subtracted from each other to obtain differences.
    print() or str() the object to show a ``ccache -s`` like output
    of the captured stats.

    """

    STATS_KEYS = [
        # (key, description)
        # Refer to stats.c in ccache project for all the descriptions.
        ("stats_zeroed", "stats zero time"),  # Old name prior to ccache 3.4
        ("stats_zeroed", "stats zeroed"),
        ("stats_updated", "stats updated"),
        ("cache_hit_direct", "cache hit (direct)"),
        ("cache_hit_preprocessed", "cache hit (preprocessed)"),
        ("cache_hit_rate", "cache hit rate"),
        ("cache_miss", "cache miss"),
        ("link", "called for link"),
        ("preprocessing", "called for preprocessing"),
        ("multiple", "multiple source files"),
        ("stdout", "compiler produced stdout"),
        ("no_output", "compiler produced no output"),
        ("empty_output", "compiler produced empty output"),
        ("failed", "compile failed"),
        ("error", "ccache internal error"),
        ("preprocessor_error", "preprocessor error"),
        ("cant_use_pch", "can't use precompiled header"),
        ("compiler_missing", "couldn't find the compiler"),
        ("cache_file_missing", "cache file missing"),
        ("bad_args", "bad compiler arguments"),
        ("unsupported_lang", "unsupported source language"),
        ("compiler_check_failed", "compiler check failed"),
        ("autoconf", "autoconf compile/link"),
        ("unsupported_code_directive", "unsupported code directive"),
        ("unsupported_compiler_option", "unsupported compiler option"),
        ("out_stdout", "output to stdout"),
        ("out_device", "output to a non-regular file"),
        ("no_input", "no input file"),
        ("bad_extra_file", "error hashing extra file"),
        ("num_cleanups", "cleanups performed"),
        ("cache_files", "files in cache"),
        ("cache_size", "cache size"),
        ("cache_max_size", "max cache size"),
    ]

    DIRECTORY_DESCRIPTION = "cache directory"
    PRIMARY_CONFIG_DESCRIPTION = "primary config"
    SECONDARY_CONFIG_DESCRIPTION = "secondary config"

    STATS_KEYS_4_4 = [
        ("stats_updated", "Summary/Stats updated"),
        (
            "cache_hit_rate",
            "Summary/Hits",
            lambda x: next(iter(re.findall(r"\((.*) %\)", x)), "0.00 %"),
        ),
        (
            "cache_hit_direct",
            "Summary/Hits/Direct",
            lambda x: next(iter(re.findall(r"(\d+)\s+/\s+\d+\s+\(", x)), "0"),
        ),
        (
            "cache_hit_preprocessed",
            "Summary/Hits/Preprocessed",
            lambda x: next(iter(re.findall(r"(\d+)\s+/\s+\d+\s+\(", x)), "0"),
        ),
        ("cache_miss", "Summary/Misses"),
        ("error", "Summary/Errors"),
        ("linking", "Uncacheable/Called for linking"),
        ("preprocessing", "Uncacheable/Called for preprocessing"),
        ("failed", "Uncacheable/Compilation failed"),
        ("preprocessor_error", "Uncacheable/Preprocessing failed"),
        ("cache_file_missing", "Errors/Missing cache file"),
        ("internal_error", "Errors/Internal error"),
        ("autoconf", "Uncacheable/Autoconf compile/link"),
        ("no_input", "Uncacheable/No input file"),
        ("unsupported_code_directive", "Uncacheable/Unsupported code directive"),
        ("unsupported_compiler_options", "Uncacheable/Unsupported compiler option"),
        ("could_not_use_precompiled", "Uncacheable/Could not use precompiled header"),
        ("compiler_stdout", "Uncacheable/Compiler produced stdout"),
        ("unsupported_language", "Uncacheable/Unsupported source language"),
        ("bad_args", "Uncacheable/Bad compiler arguments"),
        ("num_cleanups", "Primary storage/Cleanups"),
        ("cache_files", "Primary storage/Files"),
        # Cache size is reported in GB, see
        # https://github.com/ccache/ccache/commit/8892814e8a790d615e44262c0005513d6d49f9e1#diff-30ec2bbfafe4c3842c8e35179ef0d3253a66dcc054812e50049d0ca3b10e317fR274
        (
            "cache_size",
            "Primary storage/Cache size (GB)",
            lambda x: str(
                float(next(iter(re.findall(r"(.*)\s+/\s+.*\s+\(", x)), "0"))
                * CCacheStats.GiB
            ),
            True,  # Allow to continue evaluation for the next value
        ),
        (
            "cache_max_size",
            "Primary storage/Cache size (GB)",
            lambda x: str(
                float(next(iter(re.findall(r".*\s+/\s+(.*)\s+\(", x)), "0"))
                * CCacheStats.GiB
            ),
        ),
    ]

    SKIP_KEYS_4_4 = [
        "Summary/Uncacheable",
        "Summary/Misses/Direct",
        "Summary/Misses/Preprocessed",
        "Primary storage/Hits",
        "Primary storage/Misses",
        "Errors/Could not find compiler",
    ]

    DIRECTORY_DESCRIPTION_4_4 = "Summary/Cache directory"
    PRIMARY_CONFIG_DESCRIPTION_4_4 = "Summary/Primary config"
    SECONDARY_CONFIG_DESCRIPTION_4_4 = "Summary/Secondary config"

    ABSOLUTE_KEYS = {"cache_files", "cache_size", "cache_max_size"}
    FORMAT_KEYS = {"cache_size", "cache_max_size"}

    GiB = 1024 ** 3
    MiB = 1024 ** 2
    KiB = 1024

    def __init__(self, output=None, is_version_4_4_or_newer=False):
        """Construct an instance from the output of ccache -s."""
        self._values = {}
        self.cache_dir = ""
        self.primary_config = ""
        self.secondary_config = ""

        if not output:
            return

        if is_version_4_4_or_newer:
            self._parse_human_format_4_4_plus(output)
        else:
            self._parse_human_format(output)

    def _parse_human_format_4_4_plus(self, content):
        head = ""
        subhead = ""

        for line in content.splitlines():
            name = ""
            if not line:
                continue
            if line.startswith("  "):
                if not line.startswith("    "):
                    subhead = line.strip().split(":")[0]
                name = line.strip().split(":")[0]
                raw_value = ":".join(line.split(":")[1:]).strip()
                if raw_value:
                    key = head
                    if subhead != name:
                        key += "/{}".format(subhead)
                    key += "/{}".format(name)
                    self._parse_line_4_4_plus(key, raw_value)
            else:
                head = line.strip(":")
                subhead = ""

    def _parse_line_4_4_plus(self, key, value):
        if key.startswith(self.DIRECTORY_DESCRIPTION_4_4):
            self.cache_dir = value
        elif key.startswith(self.PRIMARY_CONFIG_DESCRIPTION_4_4):
            self.primary_config = value
        elif key.startswith(self.SECONDARY_CONFIG_DESCRIPTION_4_4):
            self.secondary_config = value
        else:
            for seq in self.STATS_KEYS_4_4:
                stat_key = seq[0]
                stat_description = seq[1]
                raw_value = value
                if len(seq) > 2:
                    raw_value = seq[2](value)
                if stat_key not in self._values and key == stat_description:
                    self._values[stat_key] = self._parse_value(raw_value)

                    # We dont want to break when we need to extract two infos
                    # from the same line
                    if len(seq) < 4:
                        break
            else:
                if key not in self.SKIP_KEYS_4_4:
                    raise ValueError(
                        "Failed to parse ccache stats output: '{}' '{}'".format(
                            key, value
                        )
                    )

    def _parse_human_format(self, output):
        for line in output.splitlines():
            line = line.strip()
            if line:
                self._parse_line(line)

    def _parse_line(self, line):
        line = six.ensure_text(line)
        if line.startswith(self.DIRECTORY_DESCRIPTION):
            self.cache_dir = self._strip_prefix(line, self.DIRECTORY_DESCRIPTION)
        elif line.startswith(self.PRIMARY_CONFIG_DESCRIPTION):
            self.primary_config = self._strip_prefix(
                line, self.PRIMARY_CONFIG_DESCRIPTION
            )
        elif line.startswith(self.SECONDARY_CONFIG_DESCRIPTION):
            self.secondary_config = self._strip_prefix(
                self._strip_prefix(line, self.SECONDARY_CONFIG_DESCRIPTION),
                "(readonly)",
            )
        else:
            for stat_key, stat_description in self.STATS_KEYS:
                if line.startswith(stat_description):
                    raw_value = self._strip_prefix(line, stat_description)
                    self._values[stat_key] = self._parse_value(raw_value)
                    break
            else:
                raise ValueError("Failed to parse ccache stats output: %s" % line)

    @staticmethod
    def _strip_prefix(line, prefix):
        return line[len(prefix) :].strip() if line.startswith(prefix) else line

    @staticmethod
    def _parse_value(raw_value):
        try:
            # ccache calls strftime with '%c' (src/stats.c)
            ts = time.strptime(raw_value, "%c")
            return int(time.mktime(ts))
        except ValueError:
            if raw_value == "never":
                return 0
            pass

        value = raw_value.split()
        unit = ""
        if len(value) == 1:
            numeric = value[0]
        elif len(value) == 2:
            numeric, unit = value
        else:
            raise ValueError("Failed to parse ccache stats value: %s" % raw_value)

        if "." in numeric:
            numeric = float(numeric)
        else:
            numeric = int(numeric)

        if unit in ("GB", "Gbytes"):
            unit = CCacheStats.GiB
        elif unit in ("MB", "Mbytes"):
            unit = CCacheStats.MiB
        elif unit in ("KB", "Kbytes"):
            unit = CCacheStats.KiB
        else:
            unit = 1

        return int(numeric * unit)

    def hit_rate_message(self):
        return (
            "ccache (direct) hit rate: {:.1%}; (preprocessed) hit rate: {:.1%};"
            " miss rate: {:.1%}".format(*self.hit_rates())
        )

    def hit_rates(self):
        direct = self._values["cache_hit_direct"]
        preprocessed = self._values["cache_hit_preprocessed"]
        miss = self._values["cache_miss"]
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
            lines.append(
                "%s%s" % (self.DIRECTORY_DESCRIPTION.ljust(LEFT_ALIGN), self.cache_dir)
            )

        for stat_key, stat_description in self.STATS_KEYS:
            if stat_key not in self._values:
                continue

            value = self._values[stat_key]

            if stat_key in self.FORMAT_KEYS:
                value = "%15s" % self._format_value(value)
            else:
                value = "%8u" % value

            lines.append("%s%s" % (stat_description.ljust(LEFT_ALIGN), value))

        return "\n".join(lines)

    def __nonzero__(self):
        relative_values = [
            v for k, v in self._values.items() if k not in self.ABSOLUTE_KEYS
        ]
        return all(v >= 0 for v in relative_values) and any(
            v > 0 for v in relative_values
        )

    def __bool__(self):
        return self.__nonzero__()

    @staticmethod
    def _format_value(v):
        if v > CCacheStats.GiB:
            return "%.1f Gbytes" % (float(v) / CCacheStats.GiB)
        elif v > CCacheStats.MiB:
            return "%.1f Mbytes" % (float(v) / CCacheStats.MiB)
        else:
            return "%.1f Kbytes" % (float(v) / CCacheStats.KiB)

    @staticmethod
    def check_version_4_4_or_newer(ccache):
        output_version = subprocess.check_output(
            [ccache, "--version"], universal_newlines=True
        )
        return CCacheStats._is_version_4_4_or_newer(output_version)

    @staticmethod
    def _is_version_4_4_or_newer(output):
        if "ccache version" not in output:
            return False

        major = 0
        minor = 0

        for line in output.splitlines():
            version = re.search(r"ccache version (\d+).(\d+).*", line)
            if version:
                major = int(version.group(1))
                minor = int(version.group(2))
                break

        return ((major << 8) + minor) >= ((4 << 8) + 4)


class BuildDriver(MozbuildObject):
    """Provides a high-level API for build actions."""

    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, virtualenv_name="build", **kwargs)
        self.metrics = None
        self.mach_context = None

    def build(
        self,
        metrics,
        what=None,
        jobs=0,
        job_size=0,
        directory=None,
        verbose=False,
        keep_going=False,
        mach_context=None,
        append_env=None,
        virtualenv_topobjdir=None,
    ):
        """Invoke the build backend.

        ``what`` defines the thing to build. If not defined, the default
        target is used.
        """
        self.metrics = metrics
        self.mach_context = mach_context
        warnings_path = self._get_state_filename("warnings.json")
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
                print("Can only use -C/--directory with an explicit target " "name.")
                return 1

            if directory is not None:
                directory = mozpath.normsep(directory)
                if directory.startswith("/"):
                    directory = directory[1:]

            monitor.start_resource_recording()

            if self._check_clobber(self.mozconfig, os.environ):
                return 1

            self.mach_context.command_attrs["clobber"] = False
            self.metrics.mozbuild.clobber.set(False)
            config = None
            try:
                config = self.config_environment
            except Exception:
                # If we don't already have a config environment this is either
                # a fresh objdir or $OBJDIR/config.status has been removed for
                # some reason, which indicates a clobber of sorts.
                self.mach_context.command_attrs["clobber"] = True
                self.metrics.mozbuild.clobber.set(True)

            # Record whether a clobber was requested so we can print
            # a special message later if the build fails.
            clobber_requested = False

            # Write out any changes to the current mozconfig in case
            # they should invalidate configure.
            self._write_mozconfig_json()

            previous_backend = None
            if config is not None:
                previous_backend = config.substs.get("BUILD_BACKENDS", [None])[0]

            config_rc = None
            # Even if we have a config object, it may be out of date
            # if something that influences its result has changed.
            if config is None or self.build_out_of_date(
                mozpath.join(self.topobjdir, "config.status"),
                mozpath.join(self.topobjdir, "config_status_deps.in"),
            ):
                if previous_backend and "Make" not in previous_backend:
                    clobber_requested = self._clobber_configure()

                if config is None:
                    print(" Config object not found by mach.")

                config_rc = self.configure(
                    metrics,
                    buildstatus_messages=True,
                    line_handler=output.on_line,
                    append_env=append_env,
                    virtualenv_topobjdir=virtualenv_topobjdir,
                )

                if config_rc != 0:
                    return config_rc

                config = self.reload_config_environment()

            # Collect glean metrics
            substs = config.substs
            mozbuild_metrics = metrics.mozbuild
            mozbuild_metrics.compiler.set(substs.get("CC_TYPE", None))

            def get_substs_flag(name):
                return bool(substs.get(name, None))

            mozbuild_metrics.artifact.set(get_substs_flag("MOZ_ARTIFACT_BUILDS"))
            mozbuild_metrics.debug.set(get_substs_flag("MOZ_DEBUG"))
            mozbuild_metrics.opt.set(get_substs_flag("MOZ_OPTIMIZE"))
            mozbuild_metrics.ccache.set(get_substs_flag("CCACHE"))
            using_sccache = get_substs_flag("MOZ_USING_SCCACHE")
            mozbuild_metrics.sccache.set(using_sccache)
            mozbuild_metrics.icecream.set(get_substs_flag("CXX_IS_ICECREAM"))
            mozbuild_metrics.project.set(substs.get("MOZ_BUILD_APP", ""))

            all_backends = config.substs.get("BUILD_BACKENDS", [None])
            active_backend = all_backends[0]

            status = None

            if not config_rc and any(
                [
                    self.backend_out_of_date(
                        mozpath.join(self.topobjdir, "backend.%sBackend" % backend)
                    )
                    for backend in all_backends
                ]
            ):
                print("Build configuration changed. Regenerating backend.")
                args = [
                    config.substs["PYTHON3"],
                    mozpath.join(self.topobjdir, "config.status"),
                ]
                self.run_process(args, cwd=self.topobjdir, pass_thru=True)

            if jobs == 0:
                for param in self.mozconfig.get("make_extra") or []:
                    key, value = param.split("=", 1)
                    if key == "MOZ_PARALLEL_BUILD":
                        jobs = int(value)

            if "Make" not in active_backend:
                backend_cls = get_backend_class(active_backend)(config)
                status = backend_cls.build(self, output, jobs, verbose, what)

                if status and clobber_requested:
                    for line in CLOBBER_REQUESTED_MESSAGE.splitlines():
                        self.log(
                            logging.WARNING, "clobber", {"msg": line.rstrip()}, "{msg}"
                        )

            if what and status is None:
                # Collect target pairs.
                target_pairs = []
                for target in what:
                    path_arg = self._wrap_path_argument(target)

                    if directory is not None:
                        make_dir = os.path.join(self.topobjdir, directory)
                        make_target = target
                    else:
                        make_dir, make_target = resolve_target_to_make(
                            self.topobjdir, path_arg.relpath()
                        )

                    if make_dir is None and make_target is None:
                        return 1

                    # See bug 886162 - we don't want to "accidentally" build
                    # the entire tree (if that's really the intent, it's
                    # unlikely they would have specified a directory.)
                    if not make_dir and not make_target:
                        print(
                            "The specified directory doesn't contain a "
                            "Makefile and the first parent with one is the "
                            "root of the tree. Please specify a directory "
                            "with a Makefile or run |mach build| if you "
                            "want to build the entire tree."
                        )
                        return 1

                    target_pairs.append((make_dir, make_target))

                # Build target pairs.
                for make_dir, make_target in target_pairs:
                    # We don't display build status messages during partial
                    # tree builds because they aren't reliable there. This
                    # could potentially be fixed if the build monitor were more
                    # intelligent about encountering undefined state.
                    no_build_status = "1" if make_dir is not None else ""
                    tgt_env = dict(append_env or {})
                    tgt_env["NO_BUILDSTATUS_MESSAGES"] = no_build_status
                    status = self._run_make(
                        directory=make_dir,
                        target=make_target,
                        line_handler=output.on_line,
                        log=False,
                        print_directory=False,
                        ensure_exit_code=False,
                        num_jobs=jobs,
                        job_size=job_size,
                        silent=not verbose,
                        append_env=tgt_env,
                        keep_going=keep_going,
                    )

                    if status != 0:
                        break

            elif status is None:
                # If the backend doesn't specify a build() method, then just
                # call client.mk directly.
                status = self._run_client_mk(
                    line_handler=output.on_line,
                    jobs=jobs,
                    job_size=job_size,
                    verbose=verbose,
                    keep_going=keep_going,
                    append_env=append_env,
                )

            self.log(
                logging.WARNING,
                "warning_summary",
                {"count": len(monitor.warnings_database)},
                "{count} compiler warnings present.",
            )

            # Try to run the active build backend's post-build step, if possible.
            try:
                active_backend = config.substs.get("BUILD_BACKENDS", [None])[0]
                if active_backend:
                    backend_cls = get_backend_class(active_backend)(config)
                    new_status = backend_cls.post_build(
                        self, output, jobs, verbose, status
                    )
                    status = new_status
            except Exception as ex:
                self.log(
                    logging.DEBUG,
                    "post_build",
                    {"ex": str(ex)},
                    "Unable to run active build backend's post-build step; "
                    + "failing the build due to exception: {ex}.",
                )
                if not status:
                    # If the underlying build provided a failing status, pass
                    # it through; otherwise, fail.
                    status = 1

            record_usage = status == 0

            # On automation, only record usage for plain `mach build`
            if "MOZ_AUTOMATION" in os.environ and what:
                record_usage = False

            monitor.finish(record_usage=record_usage)

        if status == 0:
            usage = monitor.get_resource_usage()
            if usage:
                self.mach_context.command_attrs["usage"] = usage

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
            pathToThirdparty = os.path.join(
                self.topsrcdir, "tools", "rewriting", "ThirdPartyPaths.txt"
            )

            pathToGenerated = os.path.join(
                self.topsrcdir, "tools", "rewriting", "Generated.txt"
            )

            if os.path.exists(pathToThirdparty):
                with io.open(
                    pathToThirdparty, encoding="utf-8", newline="\n"
                ) as f, io.open(pathToGenerated, encoding="utf-8", newline="\n") as g:
                    # Normalize the path (no trailing /)
                    LOCAL_SUPPRESS_DIRS = tuple(
                        [line.strip("\n/") for line in f]
                        + [line.strip("\n/") for line in g]
                    )
            else:
                # For application based on gecko like thunderbird
                LOCAL_SUPPRESS_DIRS = ()

            suppressed_by_dir = Counter()

            for warning in sorted(monitor.instance_warnings):
                path = mozpath.normsep(warning["filename"])
                if path.startswith(self.topsrcdir):
                    path = path[len(self.topsrcdir) + 1 :]

                warning["normpath"] = path

                if (
                    path.startswith(LOCAL_SUPPRESS_DIRS)
                    and "MOZ_AUTOMATION" not in os.environ
                ):
                    for d in LOCAL_SUPPRESS_DIRS:
                        if path.startswith(d):
                            suppressed_by_dir[d] += 1
                            break

                    continue

                if warning["column"] is not None:
                    self.log(
                        logging.WARNING,
                        "compiler_warning",
                        warning,
                        "warning: {normpath}:{line}:{column} [{flag}] " "{message}",
                    )
                else:
                    self.log(
                        logging.WARNING,
                        "compiler_warning",
                        warning,
                        "warning: {normpath}:{line} [{flag}] {message}",
                    )

            for d, count in sorted(suppressed_by_dir.items()):
                self.log(
                    logging.WARNING,
                    "suppressed_warning",
                    {"dir": d, "count": count},
                    "(suppressed {count} warnings in {dir})",
                )

        high_finder, finder_percent = monitor.have_high_finder_usage()
        if high_finder:
            print(FINDER_SLOW_MESSAGE % finder_percent)

        ccache_end = monitor.ccache_stats()

        ccache_diff = None
        if ccache_start and ccache_end:
            ccache_diff = ccache_end - ccache_start
            if ccache_diff:
                self.log(
                    logging.INFO,
                    "ccache",
                    {"msg": ccache_diff.hit_rate_message()},
                    "{msg}",
                )

        notify_minimum_time = 300
        try:
            notify_minimum_time = int(os.environ.get("MACH_NOTIFY_MINTIME", "300"))
        except ValueError:
            # Just stick with the default
            pass

        if monitor.elapsed > notify_minimum_time:
            # Display a notification when the build completes.
            self.notify("Build complete" if not status else "Build failed")

        if status:
            if what and any(
                [target for target in what if target not in ("faster", "binaries")]
            ):
                print(
                    "Hey! Builds initiated with `mach build "
                    "$A_SPECIFIC_TARGET` may not always work, even if the "
                    "code being built is correct. Consider doing a bare "
                    "`mach build` instead."
                )
            return status

        if monitor.have_resource_usage:
            excessive, swap_in, swap_out = monitor.have_excessive_swapping()
            # if excessive:
            #    print(EXCESSIVE_SWAP_MESSAGE)

            print("To view resource usage of the build, run |mach " "resource-usage|.")

        long_build = monitor.elapsed > 1200

        if long_build:
            output.on_line(
                "We know it took a while, but your build finally finished successfully!"
            )
            if not using_sccache:
                output.on_line(
                    "If you are building Firefox often, SCCache can save you a lot "
                    "of time. You can learn more here: "
                    "https://firefox-source-docs.mozilla.org/setup/"
                    "configuring_build_options.html#sccache"
                )
        else:
            output.on_line("Your build was successful!")

        # Only for full builds because incremental builders likely don't
        # need to be burdened with this.
        if not what:
            try:
                # Fennec doesn't have useful output from just building. We should
                # arguably make the build action useful for Fennec. Another day...
                if self.substs["MOZ_BUILD_APP"] != "mobile/android":
                    print("To take your build for a test drive, run: |mach run|")
                app = self.substs["MOZ_BUILD_APP"]
                if app in ("browser", "mobile/android"):
                    print(
                        "For more information on what to do now, see "
                        "https://firefox-source-docs.mozilla.org/setup/contributing_code.html"  # noqa
                    )
            except Exception:
                # Ignore Exceptions in case we can't find config.status (such
                # as when doing OSX Universal builds)
                pass

        return status

    def configure(
        self,
        metrics,
        options=None,
        buildstatus_messages=False,
        line_handler=None,
        append_env=None,
        virtualenv_topobjdir=None,
    ):
        # Disable indexing in objdir because it is not necessary and can slow
        # down builds.
        self.metrics = metrics
        mkdir(self.topobjdir, not_indexed=True)
        self._write_mozconfig_json()

        def on_line(line):
            self.log(logging.INFO, "build_output", {"line": line}, "{line}")

        line_handler = line_handler or on_line

        append_env = dict(append_env or {})

        # Back when client.mk was used, `mk_add_options "export ..."` lines
        # from the mozconfig would spill into the configure environment, so
        # add that for backwards compatibility.
        for line in self.mozconfig["make_extra"] or []:
            if line.startswith("export "):
                k, eq, v = line[len("export ") :].partition("=")
                if eq == "=":
                    append_env[k] = v

        virtualenv_topobjdir = virtualenv_topobjdir or self.topobjdir
        build_site = CommandSiteManager.from_environment(
            self.topsrcdir,
            lambda: get_state_dir(specific_to_topsrcdir=True, topsrcdir=self.topsrcdir),
            "build",
            os.path.join(virtualenv_topobjdir, "_virtualenvs"),
        )
        build_site.ensure()

        command = [build_site.python_path, os.path.join(self.topsrcdir, "configure.py")]
        if options:
            command.extend(options)

        if buildstatus_messages:
            line_handler("BUILDSTATUS TIERS configure")
            line_handler("BUILDSTATUS TIER_START configure")

        env = os.environ.copy()
        env.update(append_env)

        with subprocess.Popen(
            command,
            cwd=self.topobjdir,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        ) as process:
            for line in process.stdout:
                line_handler(line.rstrip())
            status = process.wait()
        if buildstatus_messages:
            line_handler("BUILDSTATUS TIER_FINISH configure")
        if status:
            print('*** Fix above errors and then restart with "./mach build"')
        else:
            print("Configure complete!")
            print("Be sure to run |mach build| to pick up any changes")

        return status

    def install_tests(self):
        """Install test files."""

        if self.is_clobber_needed():
            print(
                INSTALL_TESTS_CLOBBER.format(
                    clobber_file=os.path.join(self.topobjdir, "CLOBBER")
                )
            )
            sys.exit(1)

        install_test_files(mozpath.normpath(self.topsrcdir), self.topobjdir, "_tests")

    def _clobber_configure(self):
        # This is an optimistic treatment of the CLOBBER file for when we have
        # some trust in the build system: an update to the CLOBBER file is
        # interpreted to mean that configure will fail during an incremental
        # build, which is handled by removing intermediate configure artifacts
        # and subsections of the objdir related to python and testing before
        # proceeding.
        clobberer = Clobberer(self.topsrcdir, self.topobjdir)
        clobber_output = io.StringIO()
        res = clobberer.maybe_do_clobber(os.getcwd(), False, clobber_output)
        required, performed, message = res
        assert not performed
        if not required:
            return False

        def remove_objdir_path(path):
            path = mozpath.join(self.topobjdir, path)
            self.log(
                logging.WARNING,
                "clobber",
                {"path": path},
                "CLOBBER file has been updated, removing {path}.",
            )
            mozfile.remove(path)

        # Remove files we think could cause "configure" clobber bugs.
        for f in ("old-configure.vars", "config.cache", "configure.pkl"):
            remove_objdir_path(f)
            remove_objdir_path(mozpath.join("js", "src", f))

        rm_dirs = [
            # Stale paths in our virtualenv may cause build-backend
            # to fail.
            "_virtualenvs",
            # Some tests may accumulate state in the objdir that may
            # become invalid after srcdir changes.
            "_tests",
        ]

        for d in rm_dirs:
            remove_objdir_path(d)

        os.utime(mozpath.join(self.topobjdir, "CLOBBER"), None)
        return True

    def _write_mozconfig_json(self):
        mozconfig_json = os.path.join(self.topobjdir, ".mozconfig.json")
        with FileAvoidWrite(mozconfig_json) as fh:
            to_write = six.ensure_text(
                json.dumps(
                    {
                        "topsrcdir": self.topsrcdir,
                        "topobjdir": self.topobjdir,
                        "mozconfig": self.mozconfig,
                    },
                    sort_keys=True,
                    indent=2,
                )
            )
            # json.dumps in python2 inserts some trailing whitespace while
            # json.dumps in python3 does not, which defeats the FileAvoidWrite
            # mechanism. Strip the trailing whitespace to avoid rewriting this
            # file unnecessarily.
            to_write = "\n".join([line.rstrip() for line in to_write.splitlines()])
            fh.write(to_write)

    def _run_client_mk(
        self,
        target=None,
        line_handler=None,
        jobs=0,
        job_size=0,
        verbose=None,
        keep_going=False,
        append_env=None,
    ):
        append_env = dict(append_env or {})
        append_env["TOPSRCDIR"] = self.topsrcdir

        append_env["CONFIG_GUESS"] = self.resolve_config_guess()

        mozconfig = self.mozconfig

        mozconfig_make_lines = []
        for arg in mozconfig["make_extra"] or []:
            mozconfig_make_lines.append(arg)

        if mozconfig["make_flags"]:
            mozconfig_make_lines.append(
                "MOZ_MAKE_FLAGS=%s" % " ".join(mozconfig["make_flags"])
            )
        objdir = mozpath.normsep(self.topobjdir)
        mozconfig_make_lines.append("MOZ_OBJDIR=%s" % objdir)
        mozconfig_make_lines.append("OBJDIR=%s" % objdir)

        if mozconfig["path"]:
            mozconfig_make_lines.append(
                "FOUND_MOZCONFIG=%s" % mozpath.normsep(mozconfig["path"])
            )
            mozconfig_make_lines.append("export FOUND_MOZCONFIG")

        # The .mozconfig.mk file only contains exported variables and lines with
        # UPLOAD_EXTRA_FILES.
        mozconfig_filtered_lines = [
            line
            for line in mozconfig_make_lines
            # Bug 1418122 investigate why UPLOAD_EXTRA_FILES is special and
            # remove it.
            if line.startswith("export ") or "UPLOAD_EXTRA_FILES" in line
        ]

        mozconfig_client_mk = os.path.join(self.topobjdir, ".mozconfig-client-mk")
        with FileAvoidWrite(mozconfig_client_mk) as fh:
            fh.write("\n".join(mozconfig_make_lines))

        mozconfig_mk = os.path.join(self.topobjdir, ".mozconfig.mk")
        with FileAvoidWrite(mozconfig_mk) as fh:
            fh.write("\n".join(mozconfig_filtered_lines))

        # Copy the original mozconfig to the objdir.
        mozconfig_objdir = os.path.join(self.topobjdir, ".mozconfig")
        if mozconfig["path"]:
            with open(mozconfig["path"], "r") as ifh:
                with FileAvoidWrite(mozconfig_objdir) as ofh:
                    ofh.write(ifh.read())
        else:
            try:
                os.unlink(mozconfig_objdir)
            except OSError as e:
                if e.errno != errno.ENOENT:
                    raise

        if mozconfig_make_lines:
            self.log(
                logging.WARNING,
                "mozconfig_content",
                {
                    "path": mozconfig["path"],
                    "content": "\n    ".join(mozconfig_make_lines),
                },
                "Adding make options from {path}\n    {content}",
            )

        append_env["OBJDIR"] = mozpath.normsep(self.topobjdir)

        return self._run_make(
            srcdir=True,
            filename="client.mk",
            ensure_exit_code=False,
            print_directory=False,
            target=target,
            line_handler=line_handler,
            log=False,
            num_jobs=jobs,
            job_size=job_size,
            silent=not verbose,
            keep_going=keep_going,
            append_env=append_env,
        )

    def _check_clobber(self, mozconfig, env):
        """Run `Clobberer.maybe_do_clobber`, log the result and return a status bool.

        Wraps the clobbering logic in `Clobberer.maybe_do_clobber` to provide logging
        and handling of the `AUTOCLOBBER` mozconfig option.

        Return a bool indicating whether the clobber reached an error state. For example,
        return `True` if the clobber was required but not completed, and return `False` if
        the clobber was not required and not completed.
        """
        auto_clobber = any(
            [
                env.get("AUTOCLOBBER", False),
                (mozconfig["env"] or {}).get("added", {}).get("AUTOCLOBBER", False),
                "AUTOCLOBBER=1" in (mozconfig["make_extra"] or []),
            ]
        )
        from mozbuild.base import BuildEnvironmentNotFoundException

        substs = dict()
        try:
            substs = self.substs
        except BuildEnvironmentNotFoundException:
            # We'll just use an empty substs if there is no config.
            pass
        clobberer = Clobberer(self.topsrcdir, self.topobjdir, substs)
        clobber_output = six.StringIO()
        res = clobberer.maybe_do_clobber(os.getcwd(), auto_clobber, clobber_output)
        clobber_output.seek(0)
        for line in clobber_output.readlines():
            self.log(logging.WARNING, "clobber", {"msg": line.rstrip()}, "{msg}")

        clobber_required, clobber_performed, clobber_message = res
        if clobber_required and not clobber_performed:
            for line in clobber_message.splitlines():
                self.log(logging.WARNING, "clobber", {"msg": line.rstrip()}, "{msg}")
            return True

        if clobber_performed and env.get("TINDERBOX_OUTPUT"):
            self.log(
                logging.WARNING,
                "clobber",
                {"msg": "TinderboxPrint: auto clobber"},
                "{msg}",
            )

        return False
