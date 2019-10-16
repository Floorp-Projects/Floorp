# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

def migrate(ctx):
    """Bug 1588793 - Sync Now label is blank on startup, part {index}."""

    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
"""
fxa-toolbar-sync-now =
    .label = {COPY_PATTERN(from_path, "fxa-toolbar-sync-now.label")}
""",
        from_path="browser/browser/sync.ftl"),
    )
