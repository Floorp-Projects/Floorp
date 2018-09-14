# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1411707 - Migrate the findbar XBL binding to a Custom Element, part {index}."""

    ctx.add_transforms(
        'toolkit/toolkit/main-window/findbar.ftl',
        'toolkit/toolkit/main-window/findbar.ftl',
        transforms_from(
"""
findbar-next =
    .tooltiptext = { COPY(from_path, "next.tooltip") }
findbar-previous =
    .tooltiptext = { COPY(from_path, "previous.tooltip") }

findbar-find-button-close =
    .tooltiptext = { COPY(from_path, "findCloseButton.tooltip") }

findbar-highlight-all =
    .label = { COPY(from_path, "highlightAll.label") }
    .accesskey = { COPY(from_path, "highlightAll.accesskey") }
    .tooltiptext = { COPY(from_path, "highlightAll.tooltiptext") }

findbar-case-sensitive =
    .label = { COPY(from_path, "caseSensitive.label") }
    .accesskey = { COPY(from_path, "caseSensitive.accesskey") }
    .tooltiptext = { COPY(from_path, "caseSensitive.tooltiptext") }

findbar-entire-word =
    .label = { COPY(from_path, "entireWord.label") }
    .accesskey = { COPY(from_path, "entireWord.accesskey") }
    .tooltiptext = { COPY(from_path, "entireWord.tooltiptext") }
""", from_path="toolkit/chrome/global/findbar.dtd")
    )
