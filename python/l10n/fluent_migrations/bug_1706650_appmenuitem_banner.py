# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1706650 - Split appmenuitem-update-banner3 for each notification, part {index}."""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenuitem-banner-update-downloading =
    .label = { COPY_PATTERN(ftl_path, "appmenuitem-update-banner3.label-update-downloading") }
appmenuitem-banner-update-available =
    .label = { COPY_PATTERN(ftl_path, "appmenuitem-update-banner3.label-update-available") }
appmenuitem-banner-update-manual =
    .label = { COPY_PATTERN(ftl_path, "appmenuitem-update-banner3.label-update-manual") }
appmenuitem-banner-update-unsupported =
    .label = { COPY_PATTERN(ftl_path, "appmenuitem-update-banner3.label-update-unsupported") }
appmenuitem-banner-update-restart =
    .label = { COPY_PATTERN(ftl_path, "appmenuitem-update-banner3.label-update-restart") }
""",
            ftl_path="browser/browser/appmenu.ftl",
        ),
    )
