# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, MESSAGE_REFERENCE
from fluent.migrate import COPY

def migrate(ctx):
    """Bug 1491679 - Convert a subset of the strings in extensions.dtd to using Fluent for localization, part {index}."""

    ctx.add_transforms(
    "toolkit/toolkit/about/aboutAddons.ftl",
    "toolkit/toolkit/about/aboutAddons.ftl",
    transforms_from(
"""
extensions-warning-safe-mode-label =
    .value = { COPY(from_path, "warning.safemode.label") }
extensions-warning-check-compatibility-label =
    .value = { COPY(from_path, "warning.checkcompatibility.label") }
extensions-warning-check-compatibility-enable =
    .label = { COPY(from_path, "warning.checkcompatibility.enable.label") }
    .tooltiptext = { COPY(from_path, "warning.checkcompatibility.enable.tooltip") }
extensions-warning-update-security-label =
    .value = { COPY(from_path, "warning.updatesecurity.label") }
extensions-warning-update-security-enable =
    .label = { COPY(from_path, "warning.updatesecurity.enable.label") }
    .tooltiptext = { COPY(from_path, "warning.updatesecurity.enable.tooltip") }
extensions-updates-check-for-updates =
    .label = { COPY(from_path, "updates.checkForUpdates.label") }
    .accesskey = { COPY(from_path, "updates.checkForUpdates.accesskey") }
extensions-updates-view-updates =
    .label = { COPY(from_path, "updates.viewUpdates.label") }
    .accesskey = { COPY(from_path, "updates.viewUpdates.accesskey") }
extensions-updates-update-addons-automatically =
    .label = { COPY(from_path, "updates.updateAddonsAutomatically.label") }
    .accesskey = { COPY(from_path, "updates.updateAddonsAutomatically.accesskey") }
extensions-updates-reset-updates-to-automatic =
    .label = { COPY(from_path, "updates.resetUpdatesToAutomatic.label") }
    .accesskey = { COPY(from_path, "updates.resetUpdatesToAutomatic.accesskey") }
extensions-updates-reset-updates-to-manual =
    .label = { COPY(from_path, "updates.resetUpdatesToManual.label") }
    .accesskey = { COPY(from_path, "updates.resetUpdatesToManual.accesskey") }
extensions-updates-updating =
    .value = { COPY(from_path, "updates.updating.label") }
extensions-updates-installed =
    .value = { COPY(from_path, "updates.installed.label") }
extensions-updates-downloaded =
    .value = { COPY(from_path, "updates.downloaded.label") }
extensions-updates-restart =
    .label = { COPY(from_path, "updates.restart.label") }
extensions-updates-none-found =
    .value = { COPY(from_path, "updates.noneFound.label") }
extensions-updates-manual-updates-found =
    .label = { COPY(from_path, "updates.manualUpdatesFound.label") }
extensions-updates-update-selected =
    .label = { COPY(from_path, "updates.updateSelected.label") }
    .tooltiptext = { COPY(from_path, "updates.updateSelected.tooltip") }
""", from_path="toolkit/chrome/mozapps/extensions/extensions.dtd"))

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutAddons.ftl",
        "toolkit/toolkit/about/aboutAddons.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("extensions-view-discover"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("name"),
                        value=COPY(
                            "toolkit/chrome/mozapps/extensions/extensions.dtd",
                            "view.discover.label"
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("extensions-view-discover.name")
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("extensions-view-recent-updates"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("name"),
                        value=COPY(
                            "toolkit/chrome/mozapps/extensions/extensions.dtd",
                            "view.recentUpdates.label"
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("extensions-view-recent-updates.name")
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("extensions-view-available-updates"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("name"),
                        value=COPY(
                            "toolkit/chrome/mozapps/extensions/extensions.dtd",
                            "view.availableUpdates.label"
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("extensions-view-available-updates.name")
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("extensions-warning-safe-mode-container"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("extensions-warning-safe-mode-label.value")
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("extensions-warning-check-compatibility-container"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("extensions-warning-check-compatibility-label.value")
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("extensions-warning-update-security-container"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("extensions-warning-update-security-label.value")
                                )
                            ]
                        )
                    )
                ]
            ),
        ]
    )
