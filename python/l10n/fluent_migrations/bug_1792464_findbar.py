# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1792464 - Convert findbar.properties to Fluent, part {index}."""

    source = "toolkit/chrome/global/findbar.properties"
    target = "toolkit/toolkit/main-window/findbar.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("findbar-not-found"), value=COPY(source, "NotFound")
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-wrapped-to-top"),
                value=COPY(source, "WrappedToTop"),
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-wrapped-to-bottom"),
                value=COPY(source, "WrappedToBottom"),
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-normal-find"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("placeholder"),
                        value=COPY(source, "NormalFind"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-fast-find"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("placeholder"), value=COPY(source, "FastFind")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-fast-find-links"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("placeholder"),
                        value=COPY(source, "FastFindLinks"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-case-sensitive-status"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"), value=COPY(source, "CaseSensitive")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-match-diacritics-status"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=COPY(source, "MatchDiacritics"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-entire-word-status"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"), value=COPY(source, "EntireWord")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-found-matches"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=PLURALS(
                            source,
                            "FoundMatches",
                            VARIABLE_REFERENCE("total"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {
                                    "#1": VARIABLE_REFERENCE("current"),
                                    "#2": VARIABLE_REFERENCE("total"),
                                },
                            ),
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("findbar-found-matches-count-limit"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=PLURALS(
                            source,
                            "FoundMatchesCountLimit",
                            VARIABLE_REFERENCE("limit"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {"#1": VARIABLE_REFERENCE("limit")},
                            ),
                        ),
                    )
                ],
            ),
        ],
    )
