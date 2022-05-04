# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1507471 - Convert about:addons messages in extensions.properties to Fluent, part {index}."""

    source = "toolkit/chrome/mozapps/extensions/extensions.properties"
    target = "toolkit/toolkit/about/aboutAddons.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("details-notification-incompatible"),
                value=REPLACE(
                    source,
                    "details.notification.incompatible",
                    {
                        "%1$S": VARIABLE_REFERENCE("name"),
                        "%2$S": TERM_REFERENCE("brand-short-name"),
                        "%3$S": VARIABLE_REFERENCE("version"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-unsigned-and-disabled"),
                value=REPLACE(
                    source,
                    "details.notification.unsignedAndDisabled",
                    {
                        "%1$S": VARIABLE_REFERENCE("name"),
                        "%2$S": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-unsigned-and-disabled-link"),
                value=COPY(source, "details.notification.unsigned.link"),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-unsigned"),
                value=REPLACE(
                    source,
                    "details.notification.unsigned",
                    {
                        "%1$S": VARIABLE_REFERENCE("name"),
                        "%2$S": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-unsigned-link"),
                value=COPY(source, "details.notification.unsigned.link"),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-blocked"),
                value=REPLACE(
                    source,
                    "details.notification.blocked",
                    {"%1$S": VARIABLE_REFERENCE("name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-blocked-link"),
                value=COPY(source, "details.notification.blocked.link"),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-softblocked"),
                value=REPLACE(
                    source,
                    "details.notification.softblocked",
                    {"%1$S": VARIABLE_REFERENCE("name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-softblocked-link"),
                value=COPY(source, "details.notification.softblocked.link"),
            ),
            FTL.Message(
                id=FTL.Identifier("details-notification-gmp-pending"),
                value=REPLACE(
                    source,
                    "details.notification.gmpPending",
                    {"%1$S": VARIABLE_REFERENCE("name")},
                ),
            ),
        ],
    )
