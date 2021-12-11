# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1729738 - Migrate app extension properties to Fluent, part {index}."""
    ctx.add_transforms(
        "browser/browser/appExtensionFields.ftl",
        "browser/browser/appExtensionFields.ftl",
        transforms_from(
            """
extension-firefox-compact-light-name = { COPY(from_path, "extension.firefox-compact-light@mozilla.org.name") }
extension-firefox-compact-light-description = { COPY(from_path, "extension.firefox-compact-light@mozilla.org.description") }
extension-firefox-compact-dark-name = { COPY(from_path, "extension.firefox-compact-dark@mozilla.org.name") }
extension-firefox-compact-dark-description= { COPY(from_path, "extension.firefox-compact-dark@mozilla.org.description") }
extension-firefox-alpenglow-name = { COPY(from_path, "extension.firefox-alpenglow@mozilla.org.name") }
extension-firefox-alpenglow-description = { COPY(from_path, "extension.firefox-alpenglow@mozilla.org.description") }
""",
            from_path="browser/chrome/browser/app-extension-fields.properties",
        ),
    )
    ctx.add_transforms(
        "browser/browser/appExtensionFields.ftl",
        "browser/browser/appExtensionFields.ftl",
        transforms_from(
            """
extension-default-theme-name = { COPY(from_path, "extension.default-theme@mozilla.org.name") }
extension-default-theme-description = { COPY(from_path, "extension.default-theme@mozilla.org.description") }
""",
            from_path="toolkit/chrome/global/global-extension-fields.properties",
        ),
    )
