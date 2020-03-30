
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN, COPY


def migrate(ctx):
    """Bug 1525178 - Convert about:addons sidebar to HTML, part {index}"""

    ctx.add_transforms(
        'toolkit/toolkit/about/aboutAddons.ftl',
        'toolkit/toolkit/about/aboutAddons.ftl',
        transforms_from(
"""
addon-category-discover = {COPY_PATTERN(from_path, "extensions-view-discopane.name")}
addon-category-available-updates = {COPY_PATTERN(from_path, "extensions-view-available-updates.name")}
addon-category-recent-updates = {COPY_PATTERN(from_path, "extensions-view-recent-updates.name")}
"""
        , from_path='toolkit/toolkit/about/aboutAddons.ftl')
    )

    ctx.add_transforms(
        'toolkit/toolkit/about/aboutAddons.ftl',
        'toolkit/toolkit/about/aboutAddons.ftl',
        transforms_from(
"""
addon-category-extension = {COPY(from_path, "type.extension.name")}
addon-category-theme = {COPY(from_path, "type.themes.name")}
addon-category-locale = {COPY(from_path, "type.locale.name")}
addon-category-plugin = {COPY(from_path, "type.plugin.name")}
addon-category-dictionary = {COPY(from_path, "type.dictionary.name")}
"""
        , from_path='toolkit/chrome/mozapps/extensions/extensions.properties')
    )
