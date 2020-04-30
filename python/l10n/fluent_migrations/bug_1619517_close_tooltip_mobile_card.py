# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

def migrate(ctx):
    """Bug 1619517 - Add close tooltip to mobile card, part {index}"""

    ctx.add_transforms(
        "browser/browser/protections.ftl",
        "browser/browser/protections.ftl",
        transforms_from(
    """
protections-close-button2 =
        .aria-label = {COPY_PATTERN(from_path, "protections-close-button.aria-label")}
        .title = {COPY_PATTERN(from_path, "protections-close-button.aria-label")}
    """,from_path="browser/browser/protections.ftl"),
    )
