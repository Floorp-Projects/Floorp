# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN

TARGET_FILE = "browser/browser/aboutConfig.ftl"
SOURCE_FILE = TARGET_FILE


def migrate(ctx):
    """Bug 1533863 - Move about:config buttons text to title, part {index}"""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
"""
about-config-pref-add-button =
    .title = {COPY_PATTERN(from_path, "about-config-pref-add")}
about-config-pref-toggle-button =
    .title = {COPY_PATTERN(from_path, "about-config-pref-toggle")}
about-config-pref-edit-button =
    .title = {COPY_PATTERN(from_path, "about-config-pref-edit")}
about-config-pref-save-button =
    .title = {COPY_PATTERN(from_path, "about-config-pref-save")}
about-config-pref-reset-button =
    .title = {COPY_PATTERN(from_path, "about-config-pref-reset")}
about-config-pref-delete-button =
    .title = {COPY_PATTERN(from_path, "about-config-pref-delete")}
""",
        from_path=SOURCE_FILE),
    )
