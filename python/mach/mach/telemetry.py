# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, absolute_import

import os
import sys

from mock import Mock

from mozboot.util import get_state_dir, get_mach_virtualenv_binary
from mozbuild.base import MozbuildObject, BuildEnvironmentNotFoundException
from mozbuild.telemetry import filter_args
import mozpack.path

MACH_METRICS_PATH = os.path.abspath(os.path.join(__file__, "..", "..", "metrics.yaml"))


class NoopTelemetry(object):
    def __init__(self, failed_glean_import):
        self._failed_glean_import = failed_glean_import

    def metrics(self, metrics_path):
        return Mock()

    def submit(self, is_bootstrap):
        if self._failed_glean_import and not is_bootstrap:
            print(
                "Glean could not be found, so telemetry will not be reported. "
                "You may need to run |mach bootstrap|.",
                file=sys.stderr,
            )


class GleanTelemetry(object):
    """Records and sends Telemetry using Glean.

    Metrics are defined in python/mozbuild/metrics.yaml.
    Pings are defined in python/mozbuild/pings.yaml.

    The "metrics" and "pings" properties may be replaced with no-op implementations if
    Glean isn't available. This allows consumers to report telemetry without having
    to guard against incompatible environments.
    """

    def __init__(
        self,
    ):
        self._metrics_cache = {}

    def metrics(self, metrics_path):
        if metrics_path not in self._metrics_cache:
            from glean import load_metrics

            metrics = load_metrics(metrics_path)
            self._metrics_cache[metrics_path] = metrics

        return self._metrics_cache[metrics_path]

    def submit(self, _):
        from pathlib import Path
        from glean import load_pings

        pings = load_pings(Path(__file__).parent.parent / "pings.yaml")
        pings.usage.submit()


def create_telemetry_from_environment(settings):
    """Creates and a Telemetry instance based on system details.

    If telemetry isn't enabled, the current interpreter isn't Python 3, or Glean
    can't be imported, then a "mock" telemetry instance is returned that doesn't
    set or record any data. This allows consumers to optimistically set telemetry
    data without needing to specifically handle the case where the current system
    doesn't support it.
    """

    is_mach_virtualenv = mozpack.path.normpath(sys.executable) == mozpack.path.normpath(
        get_mach_virtualenv_binary()
    )

    if not (
        is_applicable_telemetry_environment()
        # Glean is not compatible with Python 2
        and sys.version_info >= (3, 0)
        # If not using the mach virtualenv (e.g.: bootstrap uses native python)
        # then we can't guarantee that the glean package that we import is a
        # compatible version. Therefore, don't use glean.
        and is_mach_virtualenv
    ):
        return NoopTelemetry(False)

    try:
        from glean import Glean
    except ImportError:
        return NoopTelemetry(True)

    from pathlib import Path

    Glean.initialize(
        "mozilla.mach",
        "Unknown",
        is_telemetry_enabled(settings),
        data_dir=Path(get_state_dir()) / "glean",
    )
    return GleanTelemetry()


def report_invocation_metrics(telemetry, command):
    metrics = telemetry.metrics(MACH_METRICS_PATH)
    metrics.mach.command.set(command)
    metrics.mach.duration.start()

    try:
        instance = MozbuildObject.from_environment()
    except BuildEnvironmentNotFoundException:
        # Mach may be invoked with the state dir as the current working
        # directory, in which case we're not able to find the topsrcdir (so
        # we can't create a MozbuildObject instance).
        # Without this information, we're unable to filter argv paths, so
        # we skip submitting them to telemetry.
        return
    metrics.mach.argv.set(
        filter_args(command, sys.argv, instance.topsrcdir, instance.topobjdir)
    )


def is_applicable_telemetry_environment():
    if os.environ.get("MACH_MAIN_PID") != str(os.getpid()):
        # This is a child mach process. Since we're collecting telemetry for the parent,
        # we don't want to collect telemetry again down here.
        return False

    if any(e in os.environ for e in ("MOZ_AUTOMATION", "TASK_ID")):
        return False

    return True


def is_telemetry_enabled(settings):
    if os.environ.get("DISABLE_TELEMETRY") == "1":
        return False

    try:
        return settings.build.telemetry
    except (AttributeError, KeyError):
        return False
