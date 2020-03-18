# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import REPLACE
from fluent.migrate.helpers import TERM_REFERENCE

def migrate(ctx):
    """Bug 1608165 - Migrate profileSelection to Fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/global/profileSelection.ftl",
        "toolkit/toolkit/global/profileSelection.ftl",
    transforms_from(
"""
profile-selection-button-cancel =
    .label = { COPY(from_path, "exit.label") }
profile-selection-new-button =
    .label = { COPY(from_path, "newButton.label") }
    .accesskey = { COPY(from_path, "newButton.accesskey") }
profile-selection-rename-button =
    .label = { COPY(from_path, "renameButton.label") }
    .accesskey = { COPY(from_path, "renameButton.accesskey") }
profile-selection-delete-button =
    .label = { COPY(from_path, "deleteButton.label") }
    .accesskey = { COPY(from_path, "deleteButton.accesskey") }
profile-manager-work-offline =
    .label = { COPY(from_path, "offlineState.label") }
    .accesskey = { COPY(from_path, "offlineState.accesskey") }
profile-manager-use-selected =
    .label = { COPY(from_path, "useSelected.label") }
    .accesskey = { COPY(from_path, "useSelected.accesskey") }
""", from_path="toolkit/chrome/mozapps/profile/profileSelection.dtd"))
    ctx.add_transforms(
    "toolkit/toolkit/global/profileSelection.ftl",
    "toolkit/toolkit/global/profileSelection.ftl",
    [
    FTL.Message(
        id=FTL.Identifier("profile-selection-window"),
        attributes=[
            FTL.Attribute(
                id=FTL.Identifier("title"),
                value=REPLACE(
                    "toolkit/chrome/mozapps/profile/profileSelection.dtd",
                    "windowtitle.label",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            )
        ]
    ),
    FTL.Message(
        id=FTL.Identifier("profile-selection-button-accept"),
        attributes=[
            FTL.Attribute(
                id=FTL.Identifier("label"),
                value=REPLACE(
                    "toolkit/chrome/mozapps/profile/profileSelection.dtd",
                    "start.label",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            )
        ]
    ),
    FTL.Message(
        id=FTL.Identifier("profile-manager-description"),
        value=REPLACE(
            "toolkit/chrome/mozapps/profile/profileSelection.dtd",
            "pmDescription.label",
            {
                "&brandShortName;": TERM_REFERENCE("brand-short-name")
            }
        )
    ),
    ]
    )
