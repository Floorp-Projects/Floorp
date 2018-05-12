# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY, CONCAT


def migrate(ctx):
    """Bug 1445084 - Migrate search results pane of Preferences to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/preferences.ftl',
        'browser/browser/preferences/preferences.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('search-input-box'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('style'),
                        CONCAT(
                            FTL.TextElement('width: '),
                            COPY(
                                'browser/chrome/browser/preferences/preferences.dtd',
                                'searchField.width'
                            )
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier('placeholder'),
                        FTL.Pattern([
                            FTL.Placeable(FTL.SelectExpression(
                                expression=FTL.CallExpression(
                                    callee=FTL.Function('PLATFORM')
                                ),
                                variants=[
                                    FTL.Variant(
                                        key=FTL.VariantName('windows'),
                                        default=False,
                                        value=COPY(
                                            'browser/chrome/browser/preferences/preferences.properties',
                                            'searchInput.labelWin'
                                        )
                                    ),
                                    FTL.Variant(
                                        key=FTL.VariantName('other'),
                                        default=True,
                                        value=COPY(
                                            'browser/chrome/browser/preferences/preferences.properties',
                                            'searchInput.labelUnix'
                                        )
                                    )
                                ]
                            ))
                        ])
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('search-results-header'),
                value=COPY(
                    'browser/chrome/browser/preferences/preferences.dtd',
                    'paneSearchResults.title'
                )
            ),
        ]
    )
