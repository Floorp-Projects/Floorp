# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

__version__ = '1.1.1'

from marionette_driver import (
    addons,
    by,
    date_time_value,
    decorators,
    errors,
    expected,
    geckoinstance,
    gestures,
    keys,
    marionette,
    selection,
    wait,
)
from marionette_driver.by import By
from marionette_driver.date_time_value import DateTimeValue
from marionette_driver.gestures import smooth_scroll, pinch
from marionette_driver.marionette import Actions
from marionette_driver.wait import Wait
