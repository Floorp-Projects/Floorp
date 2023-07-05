# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1840684 - Switch moz-message-bar to use an img with alt text instead of a pseudo element for icons, part {index}."""

    mozMessageBar = "toolkit/toolkit/global/mozMessageBar.ftl"

    ctx.add_transforms(
        mozMessageBar,
        mozMessageBar,
        transforms_from(
            """
moz-message-bar-close-button =
    .aria-label = {COPY_PATTERN(from_path, "notification-close-button.aria-label")}
    .title = {COPY_PATTERN(from_path, "notification-close-button.title")}
""",
            from_path="toolkit/toolkit/global/notification.ftl",
        ),
    )
