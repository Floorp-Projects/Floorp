# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1695792 - Update copy for FxA notifications. - part {index}"""
    ctx.add_transforms(
        "browser/browser/accounts.ftl",
        "browser/browser/accounts.ftl",
        transforms_from(
            """
account-finish-account-setup = { COPY(from_path, "account.finishAccountSetup") }
""",
            from_path="browser/chrome/browser/accounts.properties",
        ),
    )
