# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY


def migrate(ctx):
    """Bug 1786055 - Migrate styleeditor from DTD to Fluent, part {index}"""

    ctx.add_transforms(
        "devtools/client/styleeditor.ftl",
        "devtools/client/styleeditor.ftl",
        transforms_from(
            """

styleeditor-find =
    .label = { COPY(path1, "findCmd.label") }
    .accesskey = { COPY(path1, "findCmd.accesskey") }
styleeditor-find-again =
    .label = { COPY(path1, "findAgainCmd.label") }
    .accesskey = { COPY(path1, "findAgainCmd.accesskey") }
""",
            path1="toolkit/chrome/global/editMenuOverlay.dtd",
        ),
    )
