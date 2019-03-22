# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1517528 - Migrate aboutPrivateBrowsing from DTD to Fluent, part {index}"""
    
    ctx.add_transforms(
        'browser/browser/aboutPrivateBrowsing.ftl',
        'browser/browser/aboutPrivateBrowsing.ftl',
        transforms_from(
"""
about-private-browsing-info-visited = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.visited") }
about-private-browsing-search-placeholder = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.search.placeholder") }   
about-private-browsing-info-bookmarks = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.bookmarks") }      
about-private-browsing-info-title = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.title") }  
about-private-browsing-info-downloads = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.downloads") } 
about-private-browsing-info-searches = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.searches") } 
private-browsing-title = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "privateBrowsing.title") } 
about-private-browsing-not-private = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.notPrivate") } 
content-blocking-title = { COPY("browser/chrome/browser/browser.dtd", "contentBlocking.title") }   
about-private-browsing-info-myths = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.myths") } 
about-private-browsing-info-clipboard = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.clipboard") } 
about-private-browsing-info-temporary-files = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.temporaryFiles") } 
about-private-browsing-info-cookies = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "aboutPrivateBrowsing.info.cookies") } 
tracking-protection-start-tour = { COPY("browser/chrome/browser/aboutPrivateBrowsing.dtd", "trackingProtection.startTour1") } 
"""        
        )
    ),
    ctx.add_transforms(
        'browser/browser/aboutPrivateBrowsing.ftl',
        'browser/browser/aboutPrivateBrowsing.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('about-private-browsing-learn-more'),
                value=CONCAT(
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.learnMore3.before'
                    ),
                    FTL.TextElement('<a data-l10n-name="learn-more">'),
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.learnMore3.title'
                    ),
                    FTL.TextElement('</a>'),
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.learnMore3.after'
                    )
                )
            ),

            FTL.Message(
                id=FTL.Identifier('privatebrowsingpage-open-private-window-label'),
                value=COPY(
                    'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                    'privatebrowsingpage.openPrivateWindow.label',
                ),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                            'privatebrowsingpage.openPrivateWindow.accesskey',
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier('about-private-browsing-info-notsaved'),
                value=CONCAT(
                    REPLACE(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.info.notsaved.before',
                        {
                            "Firefox": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('<strong>'),
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.info.notsaved.emphasize'
                    ),
                    FTL.TextElement('</strong>'),
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.info.notsaved.after'
                    )
                )
            ),

            FTL.Message(
                id=FTL.Identifier('about-private-browsing-info-saved'),
                value=CONCAT(
                    REPLACE(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.info.saved.before',
                        {
                            "Firefox": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('<strong>'),
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.info.saved.emphasize'
                    ),
                    FTL.TextElement('</strong>'),
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.info.saved.after2'
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier('about-private-browsing-note'),
                value=CONCAT(
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.note.before'
                    ),
                    FTL.TextElement('<strong>'),
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.note.emphasize'
                    ),
                    FTL.TextElement('</strong>'),
                    COPY(
                        'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                        'aboutPrivateBrowsing.note.after'
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier('about-private-browsing'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('title'),
                        COPY(
                            'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                            'aboutPrivateBrowsing.search.placeholder',
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier('about-private-browsing-info-description'),
                value=REPLACE(
                    'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                    'aboutPrivateBrowsing.info.description',
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            ),

            FTL.Message(
                id=FTL.Identifier('content-blocking-description'),
                value=REPLACE(
                    'browser/chrome/browser/aboutPrivateBrowsing.dtd',
                    'contentBlocking.description',
                    {
                        "Firefox": TERM_REFERENCE("brand-short-name")
                    }
                )
            ),
        ]
    )
