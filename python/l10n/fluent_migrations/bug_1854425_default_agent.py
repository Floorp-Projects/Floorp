# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.transforms import REPLACE, COPY


def migrate(ctx):
    """Bug 1854425 - Migrate defaultagent_localized.ini to Fluent, part {index}."""

    source = "browser/defaultagent/defaultagent_localized.ini"
    target = "browser/browser/backgroundtasks/defaultagent.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("default-browser-agent-task-description"),
                value=REPLACE(
                    source,
                    "DefaultBrowserAgentTaskDescription",
                    {
                        "%MOZ_APP_DISPLAYNAME%": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("default-browser-notification-header-text"),
                value=REPLACE(
                    source,
                    "DefaultBrowserNotificationHeaderText",
                    {
                        "%MOZ_APP_DISPLAYNAME%": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("default-browser-notification-body-text"),
                value=REPLACE(
                    source,
                    "DefaultBrowserNotificationBodyText",
                    {
                        "%MOZ_APP_DISPLAYNAME%": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("default-browser-notification-yes-button-text"),
                value=COPY(source, "DefaultBrowserNotificationYesButtonText"),
            ),
            FTL.Message(
                id=FTL.Identifier("default-browser-notification-no-button-text"),
                value=COPY(source, "DefaultBrowserNotificationNoButtonText"),
            ),
        ],
    )
