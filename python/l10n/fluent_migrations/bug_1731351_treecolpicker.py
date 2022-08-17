# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.transforms import COPY
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1731351 - Move restoreColumnOrder.label from DTD to FTL, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/global/tree.ftl",
        "toolkit/toolkit/global/tree.ftl",
        transforms_from(
            """
tree-columnpicker-restore-order =
    .label = { COPY(from_path, "restoreColumnOrder.label") }
""",
            from_path="toolkit/chrome/global/tree.dtd",
        ),
    )
