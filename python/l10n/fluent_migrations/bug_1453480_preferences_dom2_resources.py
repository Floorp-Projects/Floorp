# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import MESSAGE_REFERENCE, EXTERNAL_ARGUMENT
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1453480 - Migrate Fluent resources to use DOM Overlays, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/preferences.ftl',
        'browser/browser/preferences/preferences.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('search-results-empty-message'),
                value=FTL.Pattern(
                    elements=[
                        FTL.Placeable(
                            expression=FTL.SelectExpression(
                                expression=FTL.CallExpression(
                                    callee=FTL.Function('PLATFORM')
                                ),
                                variants=[
                                    FTL.Variant(
                                        key=FTL.VariantName('windows'),
                                        default=False,
                                        value=REPLACE(
                                            'browser/chrome/browser/preferences/preferences.properties',
                                            'searchResults.sorryMessageWin',
                                            {
                                                '%S': FTL.TextElement('<span data-l10n-name="query"></span>')
                                            }
                                        )
                                    ),
                                    FTL.Variant(
                                        key=FTL.VariantName('other'),
                                        default=True,
                                        value=REPLACE(
                                            'browser/chrome/browser/preferences/preferences.properties',
                                            'searchResults.sorryMessageUnix',
                                            {
                                                '%S': FTL.TextElement('<span data-l10n-name="query"></span>')
                                            }
                                        )
                                    )
                                ]
                            )
                        )
                    ]
                )
            ),
            FTL.Message(
                id=FTL.Identifier('search-results-help-link'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'searchResults.needHelp3',
                    {
                        '%S': CONCAT(
                            FTL.TextElement('<a data-l10n-name="url">'),
                            REPLACE(
                                'browser/chrome/browser/preferences/preferences.properties',
                                'searchResults.needHelpSupportLink',
                                {
                                    '%S': MESSAGE_REFERENCE('-brand-short-name'),
                                }
                            ),
                            FTL.TextElement('</a>')
                        )
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('update-application-version'),
                value=CONCAT(
                    COPY(
                        'browser/chrome/browser/preferences/advanced.dtd',
                        'updateApplication.version.pre'
                    ),
                    EXTERNAL_ARGUMENT('version'),
                    COPY(
                        'browser/chrome/browser/preferences/advanced.dtd',
                        'updateApplication.version.post'
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    COPY(
                        'browser/chrome/browser/aboutDialog.dtd',
                        'releaseNotes.link'
                    ),
                    FTL.TextElement('</a>')
                )
            ),
            FTL.Message(
                id=FTL.Identifier('performance-limit-content-process-blocked-desc'),
                value=CONCAT(
                    REPLACE(
                        'browser/chrome/browser/preferences/advanced.dtd',
                        'limitContentProcessOption.disabledDescription',
                        {
                            '&brandShortName;': MESSAGE_REFERENCE('-brand-short-name')
                        }
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    COPY(
                        'browser/chrome/browser/preferences/advanced.dtd',
                        'limitContentProcessOption.disabledDescriptionLink'
                    ),
                    FTL.TextElement('</a>')
                )
            ),
            FTL.Message(
                id=FTL.Identifier('tracking-desc'),
                value=CONCAT(
                    COPY(
                        'browser/chrome/browser/preferences/privacy.dtd',
                        'trackingProtection3.description'
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    COPY(
                        'browser/chrome/browser/preferences/privacy.dtd',
                        'trackingProtectionLearnMore2.label'
                    ),
                    FTL.TextElement('</a>')
                )
            ),
        ]
    )
