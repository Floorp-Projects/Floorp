# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY, CONCAT

def migrate(ctx):
    """Bug 1451992 - Migrate Preferences::Subdialogs::Select Bookmark to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/selectBookmark.ftl',
        'browser/browser/preferences/selectBookmark.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('select-bookmark-window'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('title'),
                        COPY(
                            'browser/chrome/browser/preferences/selectBookmark.dtd',
                            'selectBookmark.title'
                        )
                    ),
                    FTL.Attribute(
                        FTL.Identifier('style'),
                        CONCAT(
                            FTL.TextElement('width: 32em;')
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('select-bookmark-desc'),
                value=COPY(
                    'browser/chrome/browser/preferences/selectBookmark.dtd',
                    'selectBookmark.label'
                )
            )
        ]
    )
