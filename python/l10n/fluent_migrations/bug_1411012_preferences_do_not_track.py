# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1411012 - Migrate Do Not Track preferences to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/preferences.ftl',
        'browser/locales/en-US/browser/preferences/preferences.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('do-not-track-description'),
                value=COPY(
                    'browser/chrome/browser/preferences/privacy.dtd',
                    'doNotTrack.description'
                )
            ),
            FTL.Message(
                id=FTL.Identifier('do-not-track-learn-more'),
                value=COPY(
                    'browser/chrome/browser/preferences/privacy.dtd',
                    'doNotTrack.learnMore.label'
                )
            ),
            FTL.Message(
                id=FTL.Identifier('do-not-track-option-default'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/privacy.dtd',
                            'doNotTrack.default.label'
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier('do-not-track-option-always'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/privacy.dtd',
                            'doNotTrack.always.label'
                        )
                    )
                ]
            ),
        ]
    )
