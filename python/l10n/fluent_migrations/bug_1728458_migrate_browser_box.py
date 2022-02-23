# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1728458 - Migrate browser-box.inc.xhtml to Fluent, part {index}"""
    target = "browser/browser/sidebarMenu.ftl"
    reference = "browser/browser/sidebarMenu.ftl"
    ctx.add_transforms(
        target,
        reference,
        transforms_from(
            """
sidebar-close-button =
    .tooltiptext = { COPY(from_path, "sidebarCloseButton.tooltip") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
