# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.transforms import REPLACE
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate import COPY, CONCAT


# Custom extension of the CONCAT migration tailored to concat
# two strings separated by a placeable.
class CONCAT_BEFORE_AFTER(CONCAT):
    def __call__(self, ctx):
        assert len(self.elements) == 3
        pattern_before, middle, pattern_after = self.elements
        elem_before = pattern_before.elements[0]
        elem_after = pattern_after.elements[0]

        if isinstance(elem_before, FTL.TextElement) and \
           len(elem_before.value) > 0 and \
           elem_before.value[-1] != " ":
            elem_before.value += " "
        if isinstance(elem_after, FTL.TextElement) and \
           len(elem_after.value) > 0 and \
           elem_after.value[0] != " ":
            elem_after.value = " " + elem_after.value
        return super(CONCAT_BEFORE_AFTER, self).__call__(ctx)

def migrate(ctx):
    """Bug 1453540 - Migrate remaining DTDs in Preferences to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/preferences.ftl',
        'browser/browser/preferences/preferences.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('translate-attribution'),
                value=CONCAT_BEFORE_AFTER(
                    COPY(
                        'browser/chrome/browser/preferences/content.dtd',
                        'translation.options.attribution.beforeLogo',
                    ),
                    FTL.TextElement('<img data-l10n-name="logo"/>'),
                    COPY(
                        'browser/chrome/browser/preferences/content.dtd',
                        'translation.options.attribution.afterLogo',
                    ),
                )
            ),
            FTL.Message(
                id=FTL.Identifier('sync-mobile-promo'),
                value=CONCAT(
                    COPY(
                        'browser/chrome/browser/preferences/sync.dtd',
                        'mobilePromo3.start',
                    ),
                    FTL.TextElement('<img data-l10n-name="android-icon"/> <a data-l10n-name="android-link">'),
                    COPY(
                        'browser/chrome/browser/preferences/sync.dtd',
                        'mobilePromo3.androidLink',
                    ),
                    FTL.TextElement('</a>'),
                    COPY(
                        'browser/chrome/browser/preferences/sync.dtd',
                        'mobilePromo3.iOSBefore',
                    ),
                    FTL.TextElement('<img data-l10n-name="ios-icon"/> <a data-l10n-name="ios-link">'),
                    COPY(
                        'browser/chrome/browser/preferences/sync.dtd',
                        'mobilePromo3.iOSLink',
                    ),
                    FTL.TextElement('</a>'),
                    COPY(
                        'browser/chrome/browser/preferences/sync.dtd',
                        'mobilePromo3.end',
                    ),
                )
            ),
            FTL.Message(
                id=FTL.Identifier('history-remember-label'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/privacy.dtd',
                    'historyHeader2.pre.label',
                    {
                        '&brandShortName;': MESSAGE_REFERENCE(
                            '-brand-short-name'
                        )
                    }
                ),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/privacy.dtd',
                            'historyHeader2.pre.accesskey',
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('history-remember-option-all'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/privacy.dtd',
                            'historyHeader.remember.label',
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('history-remember-option-never'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/privacy.dtd',
                            'historyHeader.dontremember.label',
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('history-remember-option-custom'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/privacy.dtd',
                            'historyHeader.custom.label',
                        )
                    )
                ]
            ),
        ]
    )
