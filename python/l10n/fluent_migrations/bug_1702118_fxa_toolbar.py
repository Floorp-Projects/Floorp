# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1702118 - Migrate FxA toolbar remaining strings to Fluent. - part {index}"""

    ctx.add_transforms(
        "browser/browser/sync.ftl",
        "browser/browser/sync.ftl",
        transforms_from(
            """
fxa-menu-connect-another-device =
    .label = { COPY(from_path, "fxa.menu.connectAnotherDevice2.label") }

fxa-menu-sign-out =
    .label = { COPY(from_path, "fxa.menu.signOut.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
