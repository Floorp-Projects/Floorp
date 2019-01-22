# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE


def migrate(ctx):
    """Bug 1520924 - Remove 'update' XBL binding and convert strings to Fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/updates/history.ftl",
        "toolkit/toolkit/updates/history.ftl",
        transforms_from(
"""
update-details = { COPY("toolkit/chrome/mozapps/update/updates.dtd", "update.details.label") }
""")
)

    ctx.add_transforms(
    "toolkit/toolkit/updates/history.ftl",
    "toolkit/toolkit/updates/history.ftl",
    [
        FTL.Message(
            id=FTL.Identifier("update-full-build-name"),
            value=REPLACE(
                "toolkit/chrome/mozapps/update/updates.properties",
                "updateFullName",
                {
                    "%1$S": VARIABLE_REFERENCE("name"),
                    "%2$S": VARIABLE_REFERENCE("buildID"),
                },
                normalize_printf=True
            )
        ),
        FTL.Message(
            id=FTL.Identifier("update-installed-on"),
            value=CONCAT(
                COPY(
                    "toolkit/chrome/mozapps/update/updates.dtd",
                    "update.installedOn.label"
                ),
                FTL.TextElement(" "),
                VARIABLE_REFERENCE("date"),
            ),
        ),
        FTL.Message(
            id=FTL.Identifier("update-status"),
            value=CONCAT(
                COPY(
                    "toolkit/chrome/mozapps/update/updates.dtd",
                    "update.status.label"
                ),
                FTL.TextElement(" "),
                VARIABLE_REFERENCE("status"),
            ),
        ),
    ]
)
