# coding=utf8

import fluent.syntax.ast as FTL
from fluent.migrate import (
    CONCAT, EXTERNAL_ARGUMENT, MESSAGE_REFERENCE, COPY, REPLACE
)


def migrate(ctx):
    """Migrate about:dialog, part {index}"""

    ctx.add_transforms('browser/about_dialog.ftl', 'about_dialog.ftl', [
        FTL.Message(
            id=FTL.Identifier('update-failed'),
            value=CONCAT(
                COPY(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'update.failed.start'
                ),
                FTL.TextElement('<a>'),
                COPY(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'update.failed.linkText'
                ),
                FTL.TextElement('</a>'),
                COPY(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'update.failed.end'
                )
            )
        ),
        FTL.Message(
            id=FTL.Identifier('channel-description'),
            value=CONCAT(
                COPY(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'channel.description.start'
                ),
                FTL.Placeable(EXTERNAL_ARGUMENT('channelname')),
                COPY(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'channel.description.end'
                )
            )
        ),
        FTL.Message(
            id=FTL.Identifier('community'),
            value=CONCAT(
                REPLACE(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'community.start2',
                    {
                        '&brandShortName;': MESSAGE_REFERENCE(
                            'brand-short-name'
                        )
                    }
                ),
                FTL.TextElement('<a>'),
                REPLACE(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'community.mozillaLink',
                    {
                        '&vendorShortName;': MESSAGE_REFERENCE(
                            'vendor-short-name'
                        )
                    }
                ),
                FTL.TextElement('</a>'),
                COPY(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'community.middle2'
                ),
                FTL.TextElement('<a>'),
                COPY(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'community.creditsLink'
                ),
                FTL.TextElement('</a>'),
                COPY(
                    'browser/chrome/browser/aboutDialog.dtd',
                    'community.end3'
                )
            )
        ),
    ])
