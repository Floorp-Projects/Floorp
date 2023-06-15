# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1838549 - Split sync now button strings to separate messages to avoid strange attributes, part {index}."""

    prefs = "browser/browser/preferences/preferences.ftl"

    ctx.add_transforms(
        prefs,
        prefs,
        transforms_from(
            """
prefs-sync-now-button =
    .label = {COPY_PATTERN(from_path, "prefs-sync-now.labelnotsyncing")}
    .accesskey = {COPY_PATTERN(from_path, "prefs-sync-now.accesskeynotsyncing")}

prefs-syncing-button =
    .label = {COPY_PATTERN(from_path, "prefs-sync-now.labelsyncing")}
""",
            from_path=prefs,
        ),
    )
