# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import COPY, transforms_from

def migrate(ctx):
    """Bug 1638362 - Rename urlbar-go-end-cap l10n id to urlbar-go-button, part {index}."""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
    transforms_from(
"""
urlbar-go-button =
    .tooltiptext = { COPY(from_path, "goEndCap.tooltip") }
""", from_path="browser/chrome/browser/browser.dtd"))
