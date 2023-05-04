# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1818322 - Create MozTabList and MozTabRow reusable components, part {index}."""
    ctx.add_transforms(
        "toolkit/toolkit/global/mozTabList.ftl",
        "toolkit/toolkit/global/mozTabList.ftl",
        transforms_from(
            """
mztabrow-tabs-list-tab =
   .title = {COPY_PATTERN(from_path, "firefoxview-tabs-list-tab-button.title")}
mztabrow-dismiss-tab-button =
   .title = {COPY_PATTERN(from_path, "firefoxview-closed-tabs-dismiss-tab.title")}
mztabrow-just-now-timestamp = {COPY_PATTERN(from_path, "firefoxview-just-now-timestamp")}
    """,
            from_path="browser/browser/firefoxView.ftl",
        ),
    )
