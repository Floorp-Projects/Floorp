# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1628255 - fix mistaken label attribute on buttons in download views, part {index}."""

    ctx.add_transforms(
        "browser/browser/downloads.ftl",
        "browser/browser/downloads.ftl",
        transforms_from("""
downloads-cmd-show-menuitem =
    .label = { COPY_PATTERN(from_path, "downloads-cmd-show.label") }
    .accesskey = { COPY_PATTERN(from_path, "downloads-cmd-show.accesskey") }

downloads-cmd-show-menuitem-mac =
    .label = { COPY_PATTERN(from_path, "downloads-cmd-show-mac.label") }
    .accesskey = { COPY_PATTERN(from_path, "downloads-cmd-show-mac.accesskey") }

downloads-cmd-show-button =
    .tooltiptext = { PLATFORM() ->
        [macos] { COPY_PATTERN(from_path, "downloads-cmd-show-mac.label") }
       *[other] { COPY_PATTERN(from_path, "downloads-cmd-show.label") }
    }
""", from_path="browser/browser/downloads.ftl"))
