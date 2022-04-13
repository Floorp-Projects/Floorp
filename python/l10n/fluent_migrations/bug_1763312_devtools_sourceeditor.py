# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.transforms import COPY
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1763312 - Move sourceeditor.dtd to fluent, part {index}."""

    ctx.add_transforms(
        "devtools/client/styleeditor.ftl",
        "devtools/client/styleeditor.ftl",
        transforms_from(
            """
styleeditor-go-to-line =
  .label = { COPY(from_path, "gotoLineCmd.label") }
  .accesskey = { COPY(from_path, "gotoLineCmd.accesskey") }
""",
            from_path="devtools/client/sourceeditor.dtd",
        ),
    )
