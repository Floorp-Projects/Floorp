# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1517529 - Migrate safeMode from DTD to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/safeMode.ftl',
        'browser/browser/safeMode.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('safe-mode-window'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('title'),
                        value=REPLACE(
                            'browser/chrome/browser/safeMode.dtd',
                            'safeModeDialog.title',
                            {
                                "&brandShortName;": TERM_REFERENCE("brand-short-name")
                            }
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('style'),
                        value=CONCAT(
                            FTL.TextElement('max-width: '),
                            COPY(
                                'browser/chrome/browser/safeMode.dtd',
                                'window.maxWidth'
                            ),
                            FTL.TextElement('px')
                        )
                    )
                ]
            ),
        ]
    ),
    ctx.add_transforms(
        'browser/browser/safeMode.ftl',
        'browser/browser/safeMode.ftl',
        transforms_from(
"""
start-safe-mode = 
    .label = { COPY("browser/chrome/browser/safeMode.dtd", "startSafeMode.label") }
"""
        )
    ),
    ctx.add_transforms(
        'browser/browser/safeMode.ftl',
        'browser/browser/safeMode.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('refresh-profile'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        value=REPLACE(
                            'browser/chrome/browser/safeMode.dtd',
                            'refreshProfile.label',
                            {
                                "&brandShortName;": TERM_REFERENCE("brand-short-name")
                            }
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('safe-mode-description'),
                value=REPLACE(
                    'browser/chrome/browser/safeMode.dtd',
                    'safeModeDescription3.label',
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('safe-mode-description-details'),
                value=REPLACE(
                    'browser/chrome/browser/safeMode.dtd',
                    'safeModeDescription5.label',
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('refresh-profile-instead'),
                value=REPLACE(
                    'browser/chrome/browser/safeMode.dtd',
                    'refreshProfileInstead.label',
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('auto-safe-mode-description'),
                value=REPLACE(
                    'browser/chrome/browser/safeMode.dtd',
                    'autoSafeModeDescription3.label',
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            )
        ]
    )
