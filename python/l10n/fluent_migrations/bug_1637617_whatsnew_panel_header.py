# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1637617 - Adjusted message margins to prevent panel closing; added panel header to main view panel, part {index}."""

    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from("""
whatsnew-panel-header = { COPY_PATTERN(from_path, "cfr-whatsnew-panel-header") }
""", from_path="browser/browser/newtab/asrouter.ftl"))
    