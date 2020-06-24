# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1647692 - Add language packs and dictionaries to about:support, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutSupport.ftl",
        "toolkit/toolkit/about/aboutSupport.ftl",
        transforms_from("""
support-addons-name = { COPY_PATTERN(from_path, "extensions-name") }
support-addons-version = { COPY_PATTERN(from_path, "extensions-version") }
support-addons-id = { COPY_PATTERN(from_path, "extensions-id") }
""", from_path="toolkit/toolkit/about/aboutSupport.ftl"))
