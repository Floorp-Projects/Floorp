# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1591328 - Migrate browser window title to Fluent, part {index}"""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
"""
browser-main-window-title = { $mode ->
        [private] { -brand-full-name } {COPY(from_path, "mainWindow.titlePrivateBrowsingSuffix")}
       *[default] { -brand-full-name }
    }
browser-main-window-content-title = { $mode ->
        [private] { $title } - { -brand-full-name } {COPY(from_path, "mainWindow.titlePrivateBrowsingSuffix")}
       *[default] { $title } - { -brand-full-name }
    }
""", from_path="browser/chrome/browser/browser.dtd")
    )

