# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate import REPLACE, COPY, CONCAT

def migrate(ctx):
    """Bug 1523734 - Move Strings from phishing-afterload-warning-message.dtd to Fluent, part {index}"""
    ctx.add_transforms(
        "browser/browser/safebrowsing/blockedSite.ftl",
        "browser/browser/safebrowsing/blockedSite.ftl",
        transforms_from(
"""
safeb-blocked-phishing-page-title = { COPY(from_path,"safeb.blocked.phishingPage.title3") }
safeb-blocked-malware-page-title = { COPY(from_path,"safeb.blocked.malwarePage.title2") }
safeb-blocked-unwanted-page-title = { COPY(from_path,"safeb.blocked.unwantedPage.title2") }
safeb-blocked-harmful-page-title = { COPY(from_path,"safeb.blocked.harmfulPage.title") }
safeb-palm-accept-label = { COPY(from_path,"safeb.palm.accept.label2") }
safeb-palm-see-details-label = { COPY(from_path,"safeb.palm.seedetails.label") }
safeb-palm-notdeceptive =
    .label = { COPY(from_path,"safeb.palm.notdeceptive.label") }
    .accesskey = { COPY(from_path,"safeb.palm.notdeceptive.accesskey") }
""", from_path="browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd"
        )
    )

    ctx.add_transforms(
        "browser/browser/safebrowsing/blockedSite.ftl",
        "browser/browser/safebrowsing/blockedSite.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("safeb-palm-advisory-desc"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.palm.advisory.desc2",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $advisoryname }</a>"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-malware-page-short-desc"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.malwarePage.shortDesc2",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-malware-page-error-desc-override"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.malwarePage.errorDesc.override",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $sitename }</span>"),
                        "malware_sitename": FTL.TextElement("sitename"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-malware-page-error-desc-no-override"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.malwarePage.errorDesc.noOverride",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $sitename }</span>"),
                        "malware_sitename": FTL.TextElement("sitename"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-malware-page-learn-more"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.malwarePage.learnMore",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-unwanted-page-short-desc"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.unwantedPage.shortDesc2",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-unwanted-page-error-desc-override"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.unwantedPage.errorDesc.override",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $sitename }</span>"),
                        "unwanted_sitename": FTL.TextElement("sitename"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-unwanted-page-error-desc-no-override"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.unwantedPage.errorDesc.noOverride",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $sitename }</span>"),
                        "unwanted_sitename": FTL.TextElement("sitename"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-unwanted-page-learn-more"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.unwantedPage.learnMore",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-phishing-page-short-desc"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.phishingPage.shortDesc3",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-phishing-page-error-desc-override"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.phishingPage.errorDesc.override",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $sitename }</span>"),
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "phishing_sitename": FTL.TextElement("sitename"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-phishing-page-error-desc-no-override"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.phishingPage.errorDesc.noOverride",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $sitename }</span>"),
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "phishing_sitename": FTL.TextElement("sitename"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-phishing-page-learn-more"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.phishingPage.learnMore",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-harmful-page-short-desc"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.harmfulPage.shortDesc2",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-harmful-page-error-desc-override"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.harmfulPage.errorDesc.override",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $sitename }</span>"),
                        "harmful_sitename": FTL.TextElement("sitename"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-harmful-page-error-desc-no-override"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.harmfulPage.errorDesc.noOverride",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "/>": FTL.TextElement(">{ $sitename }</span>"),
                        "harmful_sitename": FTL.TextElement("sitename"),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("safeb-blocked-harmful-page-learn-more"),
                value=REPLACE(
                    "browser/chrome/browser/safebrowsing/phishing-afterload-warning-message.dtd",
                    "safeb.blocked.harmfulPage.learnMore",
                    {
                        "id=": FTL.TextElement("data-l10n-name="),
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    }
                )
            ),
        ]
    )
