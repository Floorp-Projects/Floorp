# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE


def migrate(ctx):
    """Bug 1581185 - Migrate the pageActionContextMenu to FTL, part {index}."""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
"""
page-action-add-to-urlbar =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pageAction.addToUrlbar.label") }
page-action-manage-extension =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pageAction.manageExtension.label") }
page-action-remove-from-urlbar =
    .label = { COPY("browser/chrome/browser/browser.dtd", "pageAction.removeFromUrlbar.label") }
"""
        )
    )