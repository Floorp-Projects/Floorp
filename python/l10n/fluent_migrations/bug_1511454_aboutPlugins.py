# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY
from fluent.migrate import CONCAT

def migrate(ctx):
    """Bug 1511454 - Migrate about:plugins to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutPlugins.ftl",
        "toolkit/toolkit/about/aboutPlugins.ftl",
        transforms_from(
"""
title-label = { COPY("dom/chrome/plugins.properties", "title_label") }
installed-plugins-label = { COPY("dom/chrome/plugins.properties", "installedplugins_label") }
no-plugins-are-installed-label = { COPY("dom/chrome/plugins.properties", "nopluginsareinstalled_label") }

mime-type-label = { COPY("dom/chrome/plugins.properties", "mimetype_label") }
description-label = { COPY("dom/chrome/plugins.properties", "description_label") }
suffixes-label = { COPY("dom/chrome/plugins.properties", "suffixes_label") }
""")
)

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutPlugins.ftl",
        "toolkit/toolkit/about/aboutPlugins.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("deprecation-description"),
                value=CONCAT(
                    COPY(
                        "dom/chrome/plugins.properties",
                        "deprecation_description"
                    ),
                    FTL.TextElement(' <a data-l10n-name="deprecation-link">'),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "deprecation_learn_more"
                    ),
                    FTL.TextElement('</a>')
                )
            ),

            FTL.Message(
                id=FTL.Identifier("file-dd"),
                value=CONCAT(
                    FTL.TextElement('<span data-l10n-name="file">'),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "file_label"
                    ),
                    FTL.TextElement('</span> { $pluginLibraries }'),
                )
            ),

            FTL.Message(
                id=FTL.Identifier("path-dd"),
                value=CONCAT(
                    FTL.TextElement('<span data-l10n-name="path">'),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "path_label"
                    ),
                    FTL.TextElement('</span> { $pluginFullPath }'),
                )
            ),

            FTL.Message(
                id=FTL.Identifier("version-dd"),
                value=CONCAT(
                    FTL.TextElement('<span data-l10n-name="version">'),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "version_label"
                    ),
                    FTL.TextElement('</span> { $version }'),
                )
            ),

            FTL.Message(
                id=FTL.Identifier("state-dd-enabled"),
                value=CONCAT(
                    FTL.TextElement('<span data-l10n-name="state">'),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "state_label"
                    ),
                    FTL.TextElement('</span> '),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "state_enabled"
                    ),
                )
            ),

            FTL.Message(
                id=FTL.Identifier("state-dd-enabled-block-list-state"),
                value=CONCAT(
                    FTL.TextElement('<span data-l10n-name="state">'),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "state_label"
                    ),
                    FTL.TextElement('</span> '),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "state_enabled"
                    ),
                    FTL.TextElement(' ({ $blockListState })'),
                )
            ),

            FTL.Message(
                id=FTL.Identifier("state-dd-Disabled"),
                value=CONCAT(
                    FTL.TextElement('<span data-l10n-name="state">'),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "state_label"
                    ),
                    FTL.TextElement('</span> '),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "state_disabled"
                    ),
                )
            ),

            FTL.Message(
                id=FTL.Identifier("state-dd-Disabled-block-list-state"),
                value=CONCAT(
                    FTL.TextElement('<span data-l10n-name="state">'),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "state_label"
                    ),
                    FTL.TextElement('</span> '),
                    COPY(
                        "dom/chrome/plugins.properties",
                        "state_disabled"
                    ),
                    FTL.TextElement(' ({ $blockListState })'),
                )
            ),

        ]
    )
