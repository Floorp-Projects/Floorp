# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate import COPY
from fluent.migrate import REPLACE
from fluent.migrate import CONCAT

def migrate(ctx):
    """Bug 1505846 Migrate about:searchreset to Fluent, part {index} """

    ctx.add_transforms(

        "browser/browser/aboutSearchReset.ftl",
        "browser/browser/aboutSearchReset.ftl",
        transforms_from(
"""
tab-title = { COPY("browser/chrome/browser/aboutSearchReset.dtd", "searchreset.tabtitle") }
page-title = { COPY("browser/chrome/browser/aboutSearchReset.dtd", "searchreset.pageTitle") }
"""
        )
    )

    ctx.add_transforms(
        "browser/browser/aboutSearchReset.ftl",
        "browser/browser/aboutSearchReset.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("page-info-outofdate"),
                value=REPLACE(
                    "browser/chrome/browser/aboutSearchReset.dtd",
                    "searchreset.pageInfo1",
                    {
                        "&brandShortName;": TERM_REFERENCE("-brand-short-name"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("page-info-how-to-change"),
                value=CONCAT(
                    COPY(
                        "browser/chrome/browser/aboutSearchReset.dtd",
                        "searchreset.beforelink.pageInfo2",
                    ),
                    FTL.TextElement('<a data-l10n-name="link">'),
                    COPY(
                        "browser/chrome/browser/aboutSearchReset.dtd",
                        "searchreset.link.pageInfo2",
                    ),
                    FTL.TextElement("</a>"),
                    COPY(
                        "browser/chrome/browser/aboutSearchReset.dtd",
                        "searchreset.afterlink.pageInfo2",
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("no-change-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            "browser/chrome/browser/aboutSearchReset.dtd",
                            "searchreset.noChangeButton",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/aboutSearchReset.dtd",
                            "searchreset.noChangeButton.access",
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("change-engine-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            "browser/chrome/browser/aboutSearchReset.dtd",
                            "searchreset.changeEngineButton",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/aboutSearchReset.dtd",
                            "searchreset.changeEngineButton.access",
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("page-info-new-search-engine"),
                value=CONCAT(
                    COPY(
                        "browser/chrome/browser/aboutSearchReset.dtd",
                        "searchreset.selector.label",
                    ),
                    FTL.TextElement(' <span data-l10n-name="default-engine">{ $searchEngine }</span>'),
                )
            )
        ]
    )
