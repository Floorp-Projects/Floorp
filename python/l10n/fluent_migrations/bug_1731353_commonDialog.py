# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY


def migrate(ctx):
    """Bug 1731353 - Migrate commonDialog strings from DTD to FTL, part {index}"""

    ctx.add_transforms(
        "toolkit/toolkit/global/commonDialog.ftl",
        "toolkit/toolkit/global/commonDialog.ftl",
        transforms_from(
            """

common-dialog-copy-cmd =
    .label = { COPY(path1, "copyCmd.label") }
    .accesskey = { COPY(path1, "copyCmd.accesskey") }
common-dialog-select-all-cmd =
    .label = { COPY(path1, "selectAllCmd.label") }
    .accesskey = { COPY(path1, "selectAllCmd.accesskey") }
""",
            path1="toolkit/chrome/global/commonDialog.dtd",
        ),
    )
