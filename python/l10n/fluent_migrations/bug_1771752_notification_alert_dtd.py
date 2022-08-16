# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY


def migrate(ctx):
    """Bug 1771752 - Migrate notification alert from DTD to FTL, part {index}"""

    ctx.add_transforms(
        "toolkit/toolkit/global/alert.ftl",
        "toolkit/toolkit/global/alert.ftl",
        transforms_from(
            """

alert-close =
    .tooltiptext = { COPY(path1, "closeAlert.tooltip") }
alert-settings-title =
    .tooltiptext = { COPY(path1, "settings.label") }
""",
            path1="toolkit/chrome/alerts/alert.dtd",
        ),
    )
