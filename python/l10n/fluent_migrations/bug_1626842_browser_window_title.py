# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1626842 - Migrate browser window title to Fluent, part {index}"""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
"""
browser-main-window =
  .data-title-default = { -brand-full-name }
  .data-title-private = { -brand-full-name } {COPY(from_path, "mainWindow.titlePrivateBrowsingSuffix")}
  .data-content-title-default = { $content-title } - { -brand-full-name }
  .data-content-title-private = { $content-title } - { -brand-full-name } {COPY(from_path, "mainWindow.titlePrivateBrowsingSuffix")}

browser-main-window-mac =
  .data-title-default = { -brand-full-name }
  .data-title-private = { -brand-full-name } - {COPY(from_path, "mainWindow.titlePrivateBrowsingSuffix")}
  .data-content-title-default = { $content-title }
  .data-content-title-private = { $content-title } - {COPY(from_path, "mainWindow.titlePrivateBrowsingSuffix")}

browser-main-window-title = { -brand-full-name }

""", from_path="browser/chrome/browser/browser.dtd")
    )

