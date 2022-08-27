# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY


def migrate(ctx):
    """Bug 1786027 - Migrate appPicker.dtd to Fluent, part {index}"""

    ctx.add_transforms(
        "toolkit/toolkit/global/appPicker.ftl",
        "toolkit/toolkit/global/appPicker.ftl",
        transforms_from(
            """

app-picker-browse-button =
    .buttonlabelextra2 = { COPY(path1, "BrowseButton.label") }
app-picker-send-msg =
    .value = { COPY(path1, "SendMsg.label") }
app-picker-no-app-found =
    .value = { COPY(path1, "NoAppFound.label") }
""",
            path1="toolkit/chrome/global/appPicker.dtd",
        ),
    )
