# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import EXTERNAL_ARGUMENT
from fluent.migrate import COPY, REPLACE

def migrate(ctx):
    """Bug 1442542 - Migrate Preferences::Subdialogs::ClearSiteData properties strings to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/clearSiteData.ftl',
        'browser/browser/preferences/clearSiteData.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('clear-site-data-cookies-with-data'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        REPLACE(
                            'browser/chrome/browser/preferences/clearSiteData.properties',
                            'clearSiteDataWithEstimates.label',
                            {
                                '%1$S': EXTERNAL_ARGUMENT(
                                    'amount'
                                ),
                                '%2$S': EXTERNAL_ARGUMENT(
                                    'unit'
                                )
                            }
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'clearSiteData.accesskey'
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-cookies-empty'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'clearSiteData.label'
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'clearSiteData.accesskey'
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-cache-with-data'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        REPLACE(
                            'browser/chrome/browser/preferences/clearSiteData.properties',
                            'clearCacheWithEstimates.label',
                            {
                                '%1$S': EXTERNAL_ARGUMENT(
                                    'amount'
                                ),
                                '%2$S': EXTERNAL_ARGUMENT(
                                    'unit'
                                )
                            }
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'clearCache.accesskey'
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('clear-site-data-cache-empty'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'clearCache.label'
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/clearSiteData.dtd',
                            'clearCache.accesskey'
                        )
                    )
                ]
            )
        ]
    )
