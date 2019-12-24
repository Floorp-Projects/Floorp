# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, MESSAGE_REFERENCE
from fluent.migrate import COPY_PATTERN, REPLACE, COPY


def migrate(ctx):
    """Bug 1604960 - Migrate Text Actions to Fluent, part {index}."""

    ctx.add_transforms(
        'toolkit/toolkit/global/textActions.ftl',
        'toolkit/toolkit/global/textActions.ftl',
        transforms_from(
"""
text-action-redo =
    .label = { COPY(from_path, "redoCmd.label") }
    .accesskey = { COPY(from_path, "redoCmd.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd")
    )
