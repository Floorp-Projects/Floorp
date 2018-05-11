# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1451992 - Migrate Preferences::Subdialogs::ClearSiteData to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/clearSiteData.ftl',
        'browser/browser/preferences/clearSiteData.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('clear-site-data-window'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('title'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'window.title'
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('style'),
                        CONCAT(
                            FTL.TextElement('width: '),
                            COPY(
                                'browser/chrome/browser/preferences/clearSiteData.dtd',
                                'window.width'
                            )
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-description'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/clearSiteData.dtd',
                    'window.description',
                    {
                        '&brandShortName;': MESSAGE_REFERENCE('-brand-short-name')
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-close-key'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('key'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'windowClose.key'
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-cookies-info'),
                value=COPY(
                    'browser/chrome/browser/preferences/clearSiteData.dtd',
                    'clearSiteData.description'
                )
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-cache-info'),
                value=COPY(
                    'browser/chrome/browser/preferences/clearSiteData.dtd',
                    'clearCache.description'
                )
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-cancel'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'button.cancel.label'
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'button.cancel.accesskey'
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-clear'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'button.clear.label'
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'button.clear.accesskey'
                        )
                    )
                ]
            )
        ]
    )
