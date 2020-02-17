# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1613801 - Don't include brand name in the window title on Mac., part {index}."""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
"""
browser-main-window-content-title-default = { PLATFORM() ->
    [macos] { $title }
   *[other] { $title } - { -brand-full-name }
}

browser-main-window-content-title-private = { PLATFORM() ->
    [macos] { $title } - {COPY(from_path, "mainWindow.titlePrivateBrowsingSuffix", trim:"True")}
   *[other] { $title } - { -brand-full-name } {COPY(from_path, "mainWindow.titlePrivateBrowsingSuffix", trim:"True")}
}
""", from_path="browser/chrome/browser/browser.dtd")
    )


