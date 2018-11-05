# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import REPLACE
from fluent.migrate import COPY
from fluent.migrate import CONCAT
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import MESSAGE_REFERENCE


def migrate(ctx):
    """Bug 1491672 - Migrate About Dialog to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "browser/browser/aboutDialog.ftl",
        "browser/browser/aboutDialog.ftl",
        transforms_from(
"""
releaseNotes-link = { COPY("browser/chrome/browser/aboutDialog.dtd", "releaseNotes.link") }

update-checkForUpdatesButton =
    .label = { COPY("browser/chrome/browser/aboutDialog.dtd", "update.checkForUpdatesButton.label") }
    .accesskey = { COPY("browser/chrome/browser/aboutDialog.dtd", "update.checkForUpdatesButton.accesskey") }

update-checkingForUpdates = { COPY("browser/chrome/browser/aboutDialog.dtd", "update.checkingForUpdates")}
update-applying = { COPY("browser/chrome/browser/aboutDialog.dtd", "update.applying")}

update-adminDisabled = { COPY("browser/chrome/browser/aboutDialog.dtd", "update.adminDisabled") }

update-restarting = { COPY("browser/chrome/browser/aboutDialog.dtd", "update.restarting") }

bottomLinks-license = { COPY("browser/chrome/browser/aboutDialog.dtd", "bottomLinks.license") }
bottomLinks-rights = { COPY("browser/chrome/browser/aboutDialog.dtd", "bottomLinks.rights") }
bottomLinks-privacy = { COPY("browser/chrome/browser/aboutDialog.dtd", "bottomLinks.privacy") }

aboutDialog-architecture-sixtyFourBit = { COPY("browser/chrome/browser/browser.properties", "aboutDialog.architecture.sixtyFourBit") }
aboutDialog-architecture-thirtyTwoBit = { COPY("browser/chrome/browser/browser.properties", "aboutDialog.architecture.thirtyTwoBit") }
""")
)


    ctx.add_transforms(
        "browser/browser/aboutDialog.ftl",
        "browser/browser/aboutDialog.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("aboutDialog-title"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=REPLACE(
                                "browser/chrome/browser/aboutDialog.dtd",
                                "aboutDialog.title",
                                {
                                    "&brandFullName;": TERM_REFERENCE("-brand-full-name")
                                }
                            )
                        )
                    ]
                ),

            FTL.Message(
                id=FTL.Identifier("update-updateButton"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                                "browser/chrome/browser/aboutDialog.dtd",
                                "update.updateButton.label3",
                                {
                                    "&brandShorterName;": TERM_REFERENCE("-brand-shorter-name")
                                }
                            )
                        ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.updateButton.accesskey"
                            )
                        )
                    ]
                ),

                FTL.Message(
                    id=FTL.Identifier("update-downloading"),
                    value=CONCAT(
                        FTL.TextElement('<img data-l10n-name="icon"/>'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.downloading.start"
                        ),
                        FTL.TextElement('<label data-l10n-name="download-status"/>')
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("update-failed"),
                    value=CONCAT(
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.failed.start"
                        ),
                        FTL.TextElement('<label data-l10n-name="failed-link">'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.failed.linkText"
                        ),
                        FTL.TextElement('</label>')
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("update-failed-main"),
                    value=CONCAT(
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.failed.start"
                        ),
                        FTL.TextElement('<a data-l10n-name="failed-link-main">'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.failed.linkText"
                        ),
                        FTL.TextElement('</a>')
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("update-noUpdatesFound"),
                    value=REPLACE(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.noUpdatesFound",
                            {
                                "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                            }
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("update-otherInstanceHandlingUpdates"),
                    value=REPLACE(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.otherInstanceHandlingUpdates",
                            {
                                "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                            }
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("update-manual"),
                    value=CONCAT(
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.manual.start"
                        ),
                        FTL.TextElement('<label data-l10n-name="manual-link"/>')
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("update-unsupported"),
                    value=CONCAT(
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.unsupported.start"
                        ),
                        FTL.TextElement('<label data-l10n-name="unsupported-link">'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "update.unsupported.linkText"
                        ),
                        FTL.TextElement('</label>')
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("channel-description"),
                    value=CONCAT(
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "channel.description.start"
                        ),
                        FTL.TextElement('<label data-l10n-name="current-channel"></label>'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "channel.description.end"
                        )
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("warningDesc-version"),
                    value=REPLACE(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "warningDesc.version",
                            {
                                "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                            }
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("community-exp"),
                    value=CONCAT(
                        FTL.TextElement('<label data-l10n-name="community-exp-mozillaLink">'),
                        REPLACE(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.exp.mozillaLink",
                            {
                                "&vendorShortName;": TERM_REFERENCE("-vendor-short-name")
                            }
                        ),
                        FTL.TextElement('</label>'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.exp.middle"
                        ),
                        FTL.TextElement('<label data-l10n-name="community-exp-creditsLink">'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.exp.creditsLink"
                        ),
                        FTL.TextElement('</label>'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.exp.end"
                        )
                    )
                ),

                FTL.Message(
                    id=FTL.Identifier("community-2"),
                    value=CONCAT(
                        REPLACE(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.start2",
                            {
                                "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                            }
                        ),
                        FTL.TextElement('<label data-l10n-name="community-mozillaLink">'),
                        REPLACE(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.mozillaLink",
                            {
                                "&vendorShortName;": TERM_REFERENCE("-vendor-short-name")
                            }
                        ),
                        FTL.TextElement('</label>'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.middle2"
                        ),
                        FTL.TextElement('<label data-l10n-name="community-creditsLink">'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.creditsLink"
                        ),
                        FTL.TextElement('</label>'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "community.end3"
                        )
                    )
                ),


                FTL.Message(
                    id=FTL.Identifier("helpus"),
                    value=CONCAT(
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "helpus.start"
                        ),
                        FTL.TextElement('<label data-l10n-name="helpus-donateLink">'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "helpus.donateLink"
                        ),
                        FTL.TextElement('</label>'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "helpus.middle"
                        ),
                        FTL.TextElement('<label data-l10n-name="helpus-getInvolvedLink">'),
                        COPY(
                            "browser/chrome/browser/aboutDialog.dtd",
                            "helpus.getInvolvedLink"
                        ),
                        FTL.TextElement('</label>')
                    )
                ),
            ]
        )
    ctx.add_transforms(
        "browser/branding/official/brand.ftl",
        "browser/branding/official/brand.ftl",
        transforms_from(
"""
-brand-short-name = { COPY("browser/branding/official/brand.dtd", "brandShortName") }
-vendor-short-name = { COPY("browser/branding/official/brand.dtd", "vendorShortName") }
-brand-full-name = { COPY("browser/branding/official/brand.dtd", "brandFullName") }
-brand-shorter-name = { COPY("browser/branding/official/brand.dtd", "brandShorterName") }
trademarkInfo = { COPY("browser/branding/official/brand.dtd", "trademarkInfo.part1") }

""")
)
