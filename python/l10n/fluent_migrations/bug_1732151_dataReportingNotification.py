# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1732151 - Convert dataReportingNotification to Fluent, part {index}."""

    source = "browser/chrome/browser/browser.properties"
    target = "browser/browser/browser.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("data-reporting-notification-message"),
                value=REPLACE(
                    source,
                    "dataReportingNotification.message",
                    {
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                        "%2$S": TERM_REFERENCE("vendor-short-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("data-reporting-notification-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "dataReportingNotification.button.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            source, "dataReportingNotification.button.accessKey"
                        ),
                    ),
                ],
            ),
        ],
    )
