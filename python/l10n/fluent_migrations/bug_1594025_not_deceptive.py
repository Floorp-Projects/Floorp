# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1594025 - move 'This isn't a deceptive site' string to menubar.ftl, part {index}."""

    ctx.add_transforms(
        'browser/browser/menubar.ftl',
        'browser/browser/menubar.ftl',
        transforms_from(
"""
menu-help-not-deceptive =
    .label = {COPY_PATTERN(from_path, "safeb-palm-notdeceptive.label")}
    .accesskey = {COPY_PATTERN(from_path, "safeb-palm-notdeceptive.accesskey")}
""", from_path="browser/browser/safebrowsing/blockedSite.ftl")
    )
