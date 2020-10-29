# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1565574 - Migrate protocol handler dialog strings to fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/global/handlerDialog.ftl",
        "toolkit/toolkit/global/handlerDialog.ftl",
        transforms_from(
            """
choose-other-app-description = { COPY(from_path, "ChooseOtherApp.description") }
choose-app-btn =
      .label = { COPY(from_path, "ChooseApp.label") }
      .accessKey = { COPY(from_path, "ChooseApp.accessKey") }
""",
            from_path="toolkit/chrome/mozapps/handling/handling.dtd",
        ),
    )

    ctx.add_transforms(
        "toolkit/toolkit/global/handlerDialog.ftl",
        "toolkit/toolkit/global/handlerDialog.ftl",
        transforms_from(
            """
choose-dialog-privatebrowsing-disabled = { COPY(from_path, "privatebrowsing.disabled.label") }
choose-other-app-window-title = { COPY(from_path, "choose.application.title") }
""",
            from_path="toolkit/chrome/mozapps/handling/handling.properties",
        ),
    )
