# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1746509 - More from Mozilla XUL to HTML refactor, part {index}"""
    ctx.add_transforms(
        "browser/browser/preferences/moreFromMozilla.ftl",
        "browser/browser/preferences/moreFromMozilla.ftl",
        transforms_from(
            """
more-from-moz-button-mozilla-rally-2 =
    { COPY_PATTERN(from_path, "more-from-moz-button-mozilla-rally.label") }
more-from-moz-button-mozilla-vpn-2 =
    { COPY_PATTERN(from_path, "more-from-moz-button-mozilla-vpn.label") }
""",
            from_path="browser/browser/preferences/moreFromMozilla.ftl",
        ),
    )
