# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, MESSAGE_REFERENCE
from fluent.migrate import COPY_PATTERN, REPLACE, COPY


def migrate(ctx):
    """Bug 1605467 - Migrate text action shortcuts to Fluent, part {index}."""

    ctx.add_transforms(
        'toolkit/toolkit/global/textActions.ftl',
        'toolkit/toolkit/global/textActions.ftl',
        transforms_from(
"""
text-action-undo-shortcut =
    .key = { COPY(from_path, "undoCmd.key") }

text-action-redo-shortcut =
    .key = { COPY(from_path, "redoCmd.key") }

text-action-cut-shortcut =
    .key = { COPY(from_path, "cutCmd.key") }

text-action-copy-shortcut =
    .key = { COPY(from_path, "copyCmd.key") }

text-action-paste-shortcut =
    .key = { COPY(from_path, "pasteCmd.key") }

text-action-select-all-shortcut =
    .key = { COPY(from_path, "selectAllCmd.key") }
""", from_path="browser/chrome/browser/browser.dtd")
    )

