# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1451992 - Migrate Preferences::Subdialogs::Blocklists to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/blocklists.ftl',
        'browser/browser/preferences/blocklists.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('blocklist-window'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('title'),
                        COPY(
                            'browser/chrome/browser/preferences/blocklists.dtd',
                            'window.title',
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier('style'),
                        CONCAT(
                            FTL.TextElement('width: '),
                            COPY(
                                'browser/chrome/browser/preferences/blocklists.dtd',
                                'window.width'
                            )
                        )
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier('blocklist-desc'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'blockliststext',
                    {
                        'Firefox': MESSAGE_REFERENCE('-brand-short-name')
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier('blocklist-close-key'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('key'),
                        COPY(
                            'browser/chrome/browser/preferences/blocklists.dtd',
                            'windowClose.key',
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier('blocklist-treehead-list'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/blocklists.dtd',
                            'treehead.list.label',
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier('blocklist-button-cancel'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/blocklists.dtd',
                            'button.cancel.label',
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/blocklists.dtd',
                            'button.cancel.accesskey',
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier('blocklist-button-ok'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/blocklists.dtd',
                            'button.ok.label',
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/blocklists.dtd',
                            'button.ok.accesskey',
                        ),
                    ),
                ],
            ),
        ]
    )
