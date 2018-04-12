# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import EXTERNAL_ARGUMENT
from fluent.migrate import COPY, REPLACE

def migrate(ctx):
    """Bug 1451992 - Migrate Preferences::Subdialogs::ApplicationManager to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/applicationManager.ftl',
        'browser/browser/preferences/applicationManager.ftl',
        [
            FTL.Message(
                id=FTL.Identifier('app-manager-window'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('title'),
                        COPY(
                            'browser/chrome/browser/preferences/applicationManager.dtd',
                            'appManager.title',
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier('style'),
                        COPY(
                            'browser/chrome/browser/preferences/applicationManager.dtd',
                            'appManager.style',
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier('app-manager-remove'),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier('label'),
                        COPY(
                            'browser/chrome/browser/preferences/applicationManager.dtd',
                            'remove.label',
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier('accesskey'),
                        COPY(
                            'browser/chrome/browser/preferences/applicationManager.dtd',
                            'remove.accesskey',
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier('app-manager-handle-webfeeds'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/applicationManager.properties',
                    'descriptionApplications',
                    {
                        '%S': COPY(
                            'browser/chrome/browser/preferences/applicationManager.properties',
                            'handleWebFeeds',
                        )
                    }
                ),
            ),
            FTL.Message(
                id=FTL.Identifier('app-manager-handle-protocol'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/applicationManager.properties',
                    'descriptionApplications',
                    {
                        '%S': REPLACE(
                            'browser/chrome/browser/preferences/applicationManager.properties',
                            'handleProtocol',
                            {
                                '%S': EXTERNAL_ARGUMENT('type')
                            }
                        )
                    }
                ),
            ),
            FTL.Message(
                id=FTL.Identifier('app-manager-handle-file'),
                value=REPLACE(
                    'browser/chrome/browser/preferences/applicationManager.properties',
                    'descriptionApplications',
                    {
                        '%S': REPLACE(
                            'browser/chrome/browser/preferences/applicationManager.properties',
                            'handleFile',
                            {
                                '%S': EXTERNAL_ARGUMENT('type')
                            }
                        )
                    }
                ),
            ),
            FTL.Message(
                id=FTL.Identifier('app-manager-web-app-info'),
                value=COPY(
                    'browser/chrome/browser/preferences/applicationManager.properties',
                    'descriptionWebApp',
                ),
            ),
            FTL.Message(
                id=FTL.Identifier('app-manager-local-app-info'),
                value=COPY(
                    'browser/chrome/browser/preferences/applicationManager.properties',
                    'descriptionLocalApp',
                ),
            ),
        ]
    )
