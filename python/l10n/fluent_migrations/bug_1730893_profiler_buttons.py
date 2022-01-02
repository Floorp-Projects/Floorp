# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, COPY_PATTERN


def migrate(ctx):
    """Bug 1730893 - Localize the popup's button tooltip when changing state, part {index}"""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
profiler-popup-learn-more-button =
    .label = { COPY_PATTERN(ftl_path, "profiler-popup-learn-more") }
profiler-popup-edit-settings-button =
    .label = { COPY_PATTERN(ftl_path, "profiler-popup-edit-settings") }
profiler-popup-button-idle =
    .label = { COPY(prop_path, "profiler-button.label") }
    .tooltiptext = { COPY(prop_path, "profiler-button.tooltiptext") }
""",
            prop_path="browser/chrome/browser/customizableui/customizableWidgets.properties",
            ftl_path="browser/browser/appmenu.ftl",
        ),
    )
