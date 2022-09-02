# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY


def migrate(ctx):
    """Bug 1785216 - Migrate notification.dtd to Fluent, part {index}"""

    source = "toolkit/chrome/global/notification.dtd"
    ctx.add_transforms(
        "toolkit/toolkit/global/notification.ftl",
        "toolkit/toolkit/global/notification.ftl",
        transforms_from(
            """

close-notification-message =
    .tooltiptext = { COPY(path1, "closeNotification.tooltip") }
""",
            path1=source,
        ),
    )

    ctx.add_transforms(
        "toolkit/toolkit/global/popupnotification.ftl",
        "toolkit/toolkit/global/popupnotification.ftl",
        transforms_from(
            """

popup-notification-learn-more = { COPY(path1, "learnMoreNoEllipsis") }
popup-notification-more-actions-button =
    .aria-label = { COPY(path1, "moreActionsButton.accessibleLabel") }
popup-notification-default-button =
    .label = { COPY(path1, "defaultButton.label") }
    .accesskey = { COPY(path1, "defaultButton.accesskey") }
""",
            path1=source,
        ),
    )
