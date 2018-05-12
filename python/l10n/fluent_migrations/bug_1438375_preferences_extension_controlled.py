# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import MESSAGE_REFERENCE, EXTERNAL_ARGUMENT
from fluent.migrate import CONCAT, REPLACE


def migrate(ctx):
    """Bug 1438375 - Migrate Extension Controlled settings in Preferences to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/preferences.ftl',
        'browser/browser/preferences/preferences.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('extension-controlled-homepage-override'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'extensionControlled.homepage_override2',
                    {
                        '%S': CONCAT(
                            FTL.TextElement('<img data-l10n-name="icon"/> '),
                            EXTERNAL_ARGUMENT('name')
                        )
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('extension-controlled-new-tab-url'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'extensionControlled.newTabURL2',
                    {
                        '%S': CONCAT(
                            FTL.TextElement('<img data-l10n-name="icon"/> '),
                            EXTERNAL_ARGUMENT('name')
                        )
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('extension-controlled-default-search'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'extensionControlled.defaultSearch',
                    {
                        '%S': CONCAT(
                            FTL.TextElement('<img data-l10n-name="icon"/> '),
                            EXTERNAL_ARGUMENT('name')
                        )
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('extension-controlled-privacy-containers'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'extensionControlled.privacy.containers',
                    {
                        '%S': CONCAT(
                            FTL.TextElement('<img data-l10n-name="icon"/> '),
                            EXTERNAL_ARGUMENT('name')
                        )
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('extension-controlled-websites-tracking-protection-mode'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'extensionControlled.websites.trackingProtectionMode',
                    {
                        '%S': CONCAT(
                            FTL.TextElement('<img data-l10n-name="icon"/> '),
                            EXTERNAL_ARGUMENT('name')
                        )
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('extension-controlled-proxy-config'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'extensionControlled.proxyConfig',
                    {
                        '%1$S': CONCAT(
                            FTL.TextElement('<img data-l10n-name="icon"/> '),
                            EXTERNAL_ARGUMENT('name')
                        ),
                        '%2$S': MESSAGE_REFERENCE('-brand-short-name'),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('extension-controlled-enable'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'extensionControlled.enable',
                    {
                        '%1$S': FTL.TextElement('<img data-l10n-name="addons-icon"/>'),
                        '%2$S': FTL.TextElement('<img data-l10n-name="menu-icon"/>'),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier('network-proxy-connection-description'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/preferences.properties',
                    'connectionDesc.label',
                    {
                        '%S': MESSAGE_REFERENCE('-brand-short-name'),
                    }
                )
            ),
        ]
    )
