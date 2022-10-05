# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import PLURALS, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1793570 - Convert AndNMoreFiles string from HtmlForm.properties to Fluent, part {index}."""

    source = "dom/chrome/layout/HtmlForm.properties"
    target = "toolkit/toolkit/global/htmlForm.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("input-file-and-more-files"),
                value=PLURALS(
                    source,
                    "AndNMoreFiles",
                    VARIABLE_REFERENCE("fileCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("fileCount")},
                    ),
                ),
            )
        ],
    )
