# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import PLURALS, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1832179 - Convert sendTabsToDevice.label string to Fluent, part {index}."""

    browser = "browser/chrome/browser/browser.properties"
    target = "browser/browser/sync.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("fxa-menu-send-tab-to-device"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            browser,
                            "sendTabsToDevice.label",
                            VARIABLE_REFERENCE("tabCount"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {"#1": VARIABLE_REFERENCE("tabCount")},
                            ),
                        ),
                    )
                ],
            ),
        ],
    )
