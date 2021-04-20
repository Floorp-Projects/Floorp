# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, absolute_import

import sys
from mock import Mock


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

    Also tracks whether an employee was just automatically opted into telemetry
    during this mach invocation.
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
