
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1626381 - Add titles to about:addons sidebar, part {index}"""

    ctx.add_transforms(
        'toolkit/toolkit/about/aboutAddons.ftl',
        'toolkit/toolkit/about/aboutAddons.ftl',
        transforms_from(
"""
header-back-button =
    .title = {COPY_PATTERN(from_path, "go-back-button.tooltiptext")}

addon-category-discover-title =
    .title = {COPY_PATTERN(from_path, "addon-category-discover")}
addon-category-available-updates-title =
    .title = {COPY_PATTERN(from_path, "addon-category-available-updates")}
addon-category-recent-updates-title =
    .title = {COPY_PATTERN(from_path, "addon-category-recent-updates")}
addon-category-extension-title =
    .title = {COPY_PATTERN(from_path, "addon-category-extension")}
addon-category-theme-title =
    .title = {COPY_PATTERN(from_path, "addon-category-theme")}
addon-category-locale-title =
    .title = {COPY_PATTERN(from_path, "addon-category-locale")}
addon-category-plugin-title =
    .title = {COPY_PATTERN(from_path, "addon-category-plugin")}
addon-category-dictionary-title =
    .title = {COPY_PATTERN(from_path, "addon-category-dictionary")}

sidebar-help-button-title =
    .title = {COPY_PATTERN(from_path, "help-button")}
sidebar-preferences-button-title =
    .title = {COPY_PATTERN(from_path, "preferences")}
"""
        , from_path='toolkit/toolkit/about/aboutAddons.ftl')
    )
