# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1629832 - use tooltips on all our caption control buttons, part {index}"""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from("""
browser-window-minimize-button =
  .tooltiptext = { COPY(from_path, "fullScreenMinimize.tooltip") }
browser-window-restore-button =
  .tooltiptext = { COPY(from_path, "fullScreenRestore.tooltip") }
browser-window-close-button =
  .tooltiptext = { COPY(from_path, "fullScreenClose.tooltip") }
""", from_path="browser/chrome/browser/browser.dtd"))
