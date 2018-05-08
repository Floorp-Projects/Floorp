# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import MESSAGE_REFERENCE, EXTERNAL_ARGUMENT, transforms_from
from fluent.migrate import REPLACE


def migrate(ctx):
    """Bug 1457021 - Migrate the JS of Preferences subdialogs to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/permissions.ftl',
        'browser/browser/preferences/permissions.ftl',
        transforms_from(
"""
permissions-capabilities-allow =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "can") }
permissions-capabilities-block =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "cannot") }
permissions-capabilities-prompt =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "prompt") }
""")
    )

    ctx.add_transforms(
        'browser/browser/preferences/blocklists.ftl',
        'browser/browser/preferences/blocklists.ftl',
        transforms_from(
"""
blocklist-item-moz-std-name = { COPY("browser/chrome/browser/preferences/preferences.properties", "mozstdName") }
blocklist-item-moz-std-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "mozstdDesc") }
blocklist-item-moz-full-name = { COPY("browser/chrome/browser/preferences/preferences.properties", "mozfullName") }
blocklist-item-moz-full-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "mozfullDesc2") }
""") + [
            FTL.Message(
                id=FTL.Identifier('blocklist-item-list-template'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'mozNameTemplate',
                    {
                        '%1$S': EXTERNAL_ARGUMENT(
                            'listName'
                        ),
                        '%2$S': EXTERNAL_ARGUMENT(
                            'description'
                        )
                    }
                )
            )
        ]
    )

    ctx.add_transforms(
        'browser/browser/preferences/fonts.ftl',
        'browser/browser/preferences/fonts.ftl',
        transforms_from(
"""
fonts-very-large-warning-title = { COPY("browser/chrome/browser/preferences/preferences.properties", "veryLargeMinimumFontTitle") }
fonts-very-large-warning-message = { COPY("browser/chrome/browser/preferences/preferences.properties", "veryLargeMinimumFontWarning") }
fonts-very-large-warning-accept = { COPY("browser/chrome/browser/preferences/preferences.properties", "acceptVeryLargeMinimumFont") }
fonts-label-default-unnamed =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "labelDefaultFontUnnamed") }
""") + [
            FTL.Message(
                id=FTL.Identifier('fonts-label-default'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        REPLACE(
                            'browser/chrome/browser/preferences/preferences.properties',
                            'labelDefaultFont',
                            {
                                '%S': EXTERNAL_ARGUMENT(
                                    'name'
                                )
                            }
                        )
                    ),
                ]
            )
        ]
    )

    ctx.add_transforms(
        'browser/browser/preferences/preferences.ftl',
        'browser/browser/preferences/preferences.ftl',
        transforms_from(
"""
sitedata-total-size-calculating = { COPY("browser/chrome/browser/preferences/preferences.properties", "loadingSiteDataSize1") }
""") + [
            FTL.Message(
                id=FTL.Identifier('sitedata-total-size'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'totalSiteDataSize2',
                    {
                        '%1$S': EXTERNAL_ARGUMENT(
                            'value'
                        ),
                        '%2$S': EXTERNAL_ARGUMENT(
                            'unit'
                        )
                    }
                )
            )
        ]
    )

    ctx.add_transforms(
        'browser/browser/preferences/siteDataSettings.ftl',
        'browser/browser/preferences/siteDataSettings.ftl',
        transforms_from(
"""
site-usage-persistent = { site-usage-pattern } (Persistent)
site-data-remove-all =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "removeAllSiteData.label") }
    .accesskey = { COPY("browser/chrome/browser/preferences/preferences.properties", "removeAllSiteData.accesskey") }
site-data-remove-shown =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "removeAllSiteDataShown.label") }
    .accesskey = { COPY("browser/chrome/browser/preferences/preferences.properties", "removeAllSiteDataShown.accesskey") }
site-data-removing-dialog =
    .title = { site-data-removing-header }
    .buttonlabelaccept = { COPY("browser/chrome/browser/preferences/preferences.properties", "acceptRemove") }

""") + [
            FTL.Message(
                id=FTL.Identifier('site-data-settings-description'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'siteDataSettings3.description',
                    {
                        '%S': MESSAGE_REFERENCE(
                            '-brand-short-name'
                        )
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('site-usage-pattern'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'siteUsage',
                    {
                        '%1$S': EXTERNAL_ARGUMENT(
                            'value'
                        ),
                        '%2$S': EXTERNAL_ARGUMENT(
                            'unit'
                        )
                    }
                )
            )
        ]
    )

    ctx.add_transforms(
        'browser/browser/preferences/languages.ftl',
        'browser/browser/preferences/languages.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('languages-code-format'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        REPLACE(
                            'browser/chrome/browser/preferences/preferences.properties',
                            'languageCodeFormat',
                            {
                                '%1$S': EXTERNAL_ARGUMENT(
                                    'locale'
                                ),
                                '%2$S': EXTERNAL_ARGUMENT(
                                    'code'
                                )
                            }
                        )
                    )
                ]
            )
        ]
    )
