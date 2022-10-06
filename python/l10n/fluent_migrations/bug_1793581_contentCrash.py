# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, COPY_PATTERN, PLURALS, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1793581 - Convert ContentCrashHandlers.jsm to Fluent, part {index}."""

    source = "browser/chrome/browser/browser.properties"
    source_ftl = "browser/browser/browser.ftl"
    target = "browser/browser/contentCrash.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("crashed-subframe-message"),
                value=COPY_PATTERN(source_ftl, "crashed-subframe-message"),
            ),
            FTL.Message(
                id=FTL.Identifier("crashed-subframe-title"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY_PATTERN(source_ftl, "crashed-subframe-title.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("crashed-subframe-learnmore-link"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=COPY_PATTERN(
                            source_ftl, "crashed-subframe-learnmore-link.value"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("crashed-subframe-submit"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY_PATTERN(source_ftl, "crashed-subframe-submit.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            source_ftl, "crashed-subframe-submit.accesskey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pending-crash-reports-message"),
                value=PLURALS(
                    source,
                    "pendingCrashReports2.label",
                    VARIABLE_REFERENCE("reportCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("reportCount")},
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pending-crash-reports-view-all"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "pendingCrashReports.viewAll"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pending-crash-reports-send"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "pendingCrashReports.send"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pending-crash-reports-always-send"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "pendingCrashReports.alwaysSend"),
                    )
                ],
            ),
        ],
    )
