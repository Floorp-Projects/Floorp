# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1612723 - move reader mode l10n to use keycode, part {index}."""

    ctx.add_transforms(
        'browser/browser/browserSets.ftl',
        'browser/browser/browserSets.ftl',
        transforms_from(
"""
reader-mode-toggle-shortcut-windows =
    .keycode = { COPY(browser_path, "toggleReaderMode.win.keycode") }

reader-mode-toggle-shortcut-other =
    .key = { COPY(browser_path, "toggleReaderMode.key") }
""", browser_path="browser/chrome/browser/browser.dtd")
    )


