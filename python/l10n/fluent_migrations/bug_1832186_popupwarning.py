# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import (
    COPY,
    PLURALS,
    REPLACE,
    REPLACE_IN_TEXT,
)


def migrate(ctx):
    """Bug 1832186 - Migrate popup warning strings to Fluent, part {index}."""

    source = "browser/chrome/browser/browser.properties"
    target = "browser/browser/browser.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("popup-warning-message"),
                value=PLURALS(
                    source,
                    "popupWarning.message",
                    VARIABLE_REFERENCE("popupCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": TERM_REFERENCE("brand-short-name"),
                            "#2": VARIABLE_REFERENCE("popupCount"),
                        },
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("popup-warning-exceeded-message"),
                value=PLURALS(
                    source,
                    "popupWarning.exceeded.message",
                    VARIABLE_REFERENCE("popupCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": TERM_REFERENCE("brand-short-name"),
                            "#2": VARIABLE_REFERENCE("popupCount"),
                        },
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("popup-warning-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            [
                                FTL.Placeable(
                                    FTL.SelectExpression(
                                        selector=FTL.FunctionReference(
                                            FTL.Identifier("PLATFORM"),
                                            FTL.CallArguments(),
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.Identifier("windows"),
                                                value=COPY(
                                                    source, "popupWarningButton"
                                                ),
                                            ),
                                            FTL.Variant(
                                                key=FTL.Identifier("other"),
                                                value=COPY(
                                                    source, "popupWarningButtonUnix"
                                                ),
                                                default=True,
                                            ),
                                        ],
                                    )
                                )
                            ]
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=FTL.Pattern(
                            [
                                FTL.Placeable(
                                    FTL.SelectExpression(
                                        selector=FTL.FunctionReference(
                                            FTL.Identifier("PLATFORM"),
                                            FTL.CallArguments(),
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.Identifier("windows"),
                                                value=COPY(
                                                    source,
                                                    "popupWarningButton.accesskey",
                                                ),
                                            ),
                                            FTL.Variant(
                                                key=FTL.Identifier("other"),
                                                value=COPY(
                                                    source,
                                                    "popupWarningButtonUnix.accesskey",
                                                ),
                                                default=True,
                                            ),
                                        ],
                                    )
                                )
                            ]
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("popup-show-popup-menuitem"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "popupShowPopupPrefix",
                            {
                                "%1$S": VARIABLE_REFERENCE("popupURI"),
                                "‘": FTL.TextElement("“"),
                                "’": FTL.TextElement("”"),
                            },
                        ),
                    )
                ],
            ),
        ],
    )
