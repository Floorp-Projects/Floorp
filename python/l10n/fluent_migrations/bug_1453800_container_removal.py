# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import EXTERNAL_ARGUMENT
from fluent.migrate import COPY
from fluent.migrate.transforms import PLURALS, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1453800 - Migrate Container removal strings to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/preferences.ftl',
        'browser/browser/preferences/preferences.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('containers-remove-alert-title'),
                value=COPY(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'removeContainerAlertTitle',
                ),
            ),
            FTL.Message(
                id=FTL.Identifier('containers-remove-alert-msg'),
                value=PLURALS(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'removeContainerMsg',
                    EXTERNAL_ARGUMENT('count'),
                    lambda text: REPLACE_IN_TEXT(
                        text,
                        {
                            '#S': EXTERNAL_ARGUMENT('count')
                        }
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier('containers-remove-ok-button'),
                value=COPY(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'removeContainerOkButton',
                ),
            ),
            FTL.Message(
                id=FTL.Identifier('containers-remove-cancel-button'),
                value=COPY(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'removeContainerButton2',
                ),
            ),
        ]
    )
