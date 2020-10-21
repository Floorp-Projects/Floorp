# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import REPLACE, COPY
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE


def migrate(ctx):
    """Bug 1668284 - Unknown content type change settings label is no longer accurate, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/global/unknownContentType.ftl",
        "toolkit/toolkit/global/unknownContentType.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("unknowncontenttype-settingschange"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=FTL.SelectExpression(
                                        selector=MESSAGE_REFERENCE("PLATFORM()"),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.Identifier("windows"),
                                                default=False,
                                                value=REPLACE(
                                                    "toolkit/chrome/mozapps/downloads/settingsChange.dtd",
                                                    "settingsChangeOptions.label",
                                                    {
                                                        "&brandShortName;": TERM_REFERENCE(
                                                            "brand-short-name"
                                                        ),
                                                    },
                                                ),
                                            ),
                                            FTL.Variant(
                                                key=FTL.Identifier("other"),
                                                default=True,
                                                value=REPLACE(
                                                    "toolkit/chrome/mozapps/downloads/settingsChange.dtd",
                                                    "settingsChangePreferences.label",
                                                    {
                                                        "&brandShortName;": TERM_REFERENCE(
                                                            "brand-short-name"
                                                        ),
                                                    },
                                                ),
                                            ),
                                        ],
                                    )
                                )
                            ]
                        ),
                    )
                ],
            )
        ],
    )
