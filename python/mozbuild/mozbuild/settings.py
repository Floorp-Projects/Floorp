# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from mach.decorators import SettingsProvider


@SettingsProvider
class TelemetrySettings:
    config_settings = [
        (
            "build.telemetry",
            "boolean",
            "Enable submission of build system telemetry "
            '(Deprecated, replaced by "telemetry.is_enabled")',
        ),
        (
            "mach_telemetry.is_enabled",
            "boolean",
            "Build system telemetry is allowed",
            False,
        ),
        (
            "mach_telemetry.is_set_up",
            "boolean",
            "The telemetry setup workflow has been completed "
            "(e.g.: user has been prompted to opt-in)",
            False,
        ),
    ]
