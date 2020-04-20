# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import CONCAT, REPLACE
from fluent.migrate.helpers import COPY, TERM_REFERENCE

def migrate(ctx):
    """Bug 1523736 - Migrate updates.dtd to Fluent, part {index}."""
    ctx.add_transforms(
        "toolkit/toolkit/updates/elevation.ftl",
        "toolkit/toolkit/updates/elevation.ftl",
    transforms_from(
"""
elevation-update-wizard =
    .title = { COPY(from_path, "updateWizard.title") }
elevation-details-link-label =
    .value = { COPY(from_path, "details.link") }
elevation-finished-page = { COPY(from_path, "finishedPage.title") }
elevation-finished-background = { COPY(from_path, "finishedBackground.name") }
""",from_path="toolkit/chrome/mozapps/update/updates.dtd"))

    ctx.add_transforms(
        "toolkit/toolkit/updates/elevation.ftl",
        "toolkit/toolkit/updates/elevation.ftl",
        [
            FTL.Message(
                id = FTL.Identifier("elevation-error-manual"),
                value = REPLACE(
                    "toolkit/chrome/mozapps/update/updates.dtd",
                    "errorManual.label",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    },
                    trim = True
                )
            ),
            FTL.Message(
                id = FTL.Identifier("elevation-finished-background-page"),
                value = REPLACE(
                    "toolkit/chrome/mozapps/update/updates.dtd",
                    "finishedBackgroundPage.text",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    },
                    trim = True
                )
            ),
            FTL.Message(
                id = FTL.Identifier("elevation-more-elevated"),
                value = REPLACE(
                    "toolkit/chrome/mozapps/update/updates.dtd",
                    "finishedBackground.moreElevated",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    },
                    trim = True
                )
            ),

        ]
    )
