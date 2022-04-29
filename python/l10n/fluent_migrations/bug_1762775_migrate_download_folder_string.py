# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1762775 - download folder picker should be available all the time, part {index}"""
    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        transforms_from(
            """
download-save-where = { COPY_PATTERN(from_path, "download-save-to.label") }
    .accesskey = { COPY_PATTERN(from_path, "download-save-to.accesskey") }
""",
            from_path="browser/browser/preferences/preferences.ftl",
        ),
    )
