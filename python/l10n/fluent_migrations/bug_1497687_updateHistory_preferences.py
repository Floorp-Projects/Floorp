# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY
from fluent.migrate import REPLACE
from fluent.migrate import REPLACE_IN_TEXT
from fluent.migrate.helpers import VARIABLE_REFERENCE

def migrate(ctx):
    """Bug 1497687 - Migrate Update History in Update section of Preferences to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/updates/history.ftl",
        "toolkit/toolkit/updates/history.ftl",
        transforms_from(
"""
history-title = { COPY("toolkit/chrome/mozapps/update/history.dtd", "history.title") }
history-intro = { COPY("toolkit/chrome/mozapps/update/history.dtd", "history2.intro") }
close-button-label =
    .buttonlabelcancel = { COPY("toolkit/chrome/mozapps/update/history.dtd", "closebutton.label") }
    .title = { COPY("toolkit/chrome/mozapps/update/history.dtd", "history.title") }
no-updates-label = { COPY("toolkit/chrome/mozapps/update/history.dtd", "noupdates.label") }
name-header = { COPY("toolkit/chrome/mozapps/update/history.dtd", "name.header") }
date-header = { COPY("toolkit/chrome/mozapps/update/history.dtd", "date.header") }
type-header = { COPY("toolkit/chrome/mozapps/update/history.dtd", "type.header") }
state-header = { COPY("toolkit/chrome/mozapps/update/history.dtd", "state.header") }
""")
)

    ctx.add_transforms(
    "toolkit/toolkit/updates/history.ftl",
    "toolkit/toolkit/updates/history.ftl",
    [
        FTL.Message(
            id=FTL.Identifier("update-full-name"),
            attributes=[
                FTL.Attribute(
                    id=FTL.Identifier("name"),
                    value=REPLACE(
                        "toolkit/chrome/mozapps/update/updates.properties",
                        "updateFullName",
                        {
                            "%1$S": VARIABLE_REFERENCE("name"),
                            "%2$S": VARIABLE_REFERENCE("buildID"),
                        },
                        normalize_printf=True
                    )
                )
            ]
        )
    ]
)

