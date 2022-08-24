# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY


def migrate(ctx):
    """Bug 1734221 - Migrate datetimebox strings from DTD to FTL, part {index}"""

    ctx.add_transforms(
        "toolkit/toolkit/global/datetimebox.ftl",
        "toolkit/toolkit/global/datetimebox.ftl",
        transforms_from(
            """

datetime-reset =
    .aria-label = { COPY(path1, "datetime.reset.label") }
datetime-year-placeholder = { COPY(path1, "date.year.placeholder") }
datetime-month-placeholder = { COPY(path1, "date.month.placeholder") }
datetime-day-placeholder = { COPY(path1, "date.day.placeholder") }
datetime-time-placeholder = { COPY(path1, "time.hour.placeholder") }
datetime-year =
    .aria-label = { COPY(path1, "date.year.label") }
datetime-month =
    .aria-label = { COPY(path1, "date.month.label") }
datetime-day =
    .aria-label = { COPY(path1, "date.day.label") }
datetime-hour =
    .aria-label = { COPY(path1, "time.hour.label") }
datetime-minute =
    .aria-label = { COPY(path1, "time.minute.label") }
datetime-second =
    .aria-label = { COPY(path1, "time.second.label") }
datetime-millisecond =
    .aria-label = { COPY(path1, "time.millisecond.label") }
datetime-dayperiod =
    .aria-label = { COPY(path1, "time.dayperiod.label") }
""",
            path1="toolkit/chrome/global/datetimebox.dtd",
        ),
    )
