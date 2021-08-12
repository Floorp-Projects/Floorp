# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1724793 - Update casing of the toolbar overflow menu customize button. - part {index}"""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
toolbar-overflow-customize-button =
    .label = { COPY(from_path, "overflowCustomizeToolbar.label") }
    .accesskey = { COPY(from_path, "overflowCustomizeToolbar.accesskey") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
