# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate import COPY
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import PLURALS, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1836754 - Convert accessibility.progress.* strings to Fluent, part {index}."""

    browser = "devtools/client/accessibility.properties"
    target = "devtools/client/accessibility.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("accessibility-progress-initializing"),
                value=COPY(browser, "accessibility.progress.initializing"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-valuetext"),
                        value=COPY(browser, "accessibility.progress.initializing"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("accessibility-progress-progressbar"),
                value=PLURALS(
                    browser,
                    "accessibility.progress.progressbar",
                    VARIABLE_REFERENCE("nodeCount"),
                    lambda text: REPLACE_IN_TEXT(
                        text, {"#1": VARIABLE_REFERENCE("nodeCount")}
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("accessibility-progress-finishing"),
                value=COPY(browser, "accessibility.progress.finishing"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-valuetext"),
                        value=COPY(browser, "accessibility.progress.finishing"),
                    )
                ],
            ),
        ],
    )
