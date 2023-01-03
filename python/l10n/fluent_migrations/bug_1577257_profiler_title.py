# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1577257 - Share logic behind panel headers across the UI, part {index}"""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
profiler-popup-header-text =
    { COPY_PATTERN(from_path, "profiler-popup-title.value") }
""",
            from_path="browser/browser/appmenu.ftl",
        ),
    )
