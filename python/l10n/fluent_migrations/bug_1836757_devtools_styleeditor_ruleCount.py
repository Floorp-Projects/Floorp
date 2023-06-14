# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import PLURALS, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1836757 - Convert ruleCount.label string to Fluent, part {index}."""

    browser = "devtools/client/styleeditor.properties"
    target = "devtools/client/styleeditor.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("styleeditor-stylesheet-rule-count"),
                value=PLURALS(
                    browser,
                    "ruleCount.label",
                    VARIABLE_REFERENCE("ruleCount"),
                    lambda text: REPLACE_IN_TEXT(
                        text, {"#1": VARIABLE_REFERENCE("ruleCount")}
                    ),
                ),
            ),
        ],
    )
