# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1690573 - Port Report Site Issue WebExtension strings to Fluent, part {index}."""
    ctx.add_transforms(
        "browser/browser/menubar.ftl",
        "browser/browser/menubar.ftl",
        transforms_from(
            """
menu-help-report-site-issue =
    .label = { COPY(from_path, "wc-reporter.label2") }
""",
            from_path="browser/extensions/report-site-issue/webcompat.properties",
        ),
    )

    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenu-help-report-site-issue =
    .label = { COPY(from_path, "wc-reporter.label2") }
""",
            from_path="browser/extensions/report-site-issue/webcompat.properties",
        ),
    )
