# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1667781 - Refactor preferences dialogs to use dialog element, part {index}."""

    ctx.add_transforms(
        "browser/browser/preferences/addEngine.ftl",
        "browser/browser/preferences/addEngine.ftl",
        transforms_from("""
add-engine-dialog =
  .buttonlabelaccept = { COPY_PATTERN(from_path, "add-engine-ok.label") }
  .buttonaccesskeyaccept = { COPY_PATTERN(from_path, "add-engine-ok.accesskey") }
""", from_path="browser/browser/preferences/addEngine.ftl"))

    ctx.add_transforms(
        "browser/browser/preferences/clearSiteData.ftl",
        "browser/browser/preferences/clearSiteData.ftl",
        transforms_from("""
clear-site-data-dialog =
  .buttonlabelaccept = { COPY_PATTERN(from_path, "clear-site-data-clear.label") }
  .buttonaccesskeyaccept = { COPY_PATTERN(from_path, "clear-site-data-clear.accesskey") }
""", from_path="browser/browser/preferences/clearSiteData.ftl"))

    ctx.add_transforms(
        "browser/browser/preferences/containers.ftl",
        "browser/browser/preferences/containers.ftl",
        transforms_from("""
containers-dialog =
  .buttonlabelaccept = { COPY_PATTERN(from_path, "containers-button-done.label") }
  .buttonaccesskeyaccept = { COPY_PATTERN(from_path, "containers-button-done.accesskey") }
""", from_path="browser/browser/preferences/containers.ftl"))

    ctx.add_transforms(
        "browser/browser/preferences/permissions.ftl",
        "browser/browser/preferences/permissions.ftl",
        transforms_from("""
permission-dialog =
  .buttonlabelaccept = { COPY_PATTERN(from_path, "permissions-button-ok.label") }
  .buttonaccesskeyaccept = { COPY_PATTERN(from_path, "permissions-button-ok.accesskey") }
""", from_path="browser/browser/preferences/permissions.ftl"))

    ctx.add_transforms(
        "browser/browser/preferences/siteDataSettings.ftl",
        "browser/browser/preferences/siteDataSettings.ftl",
        transforms_from("""
site-data-settings-dialog =
  .buttonlabelaccept = { COPY_PATTERN(from_path, "site-data-button-save.label") }
  .buttonaccesskeyaccept = { COPY_PATTERN(from_path, "site-data-button-save.accesskey") }
""", from_path="browser/browser/preferences/siteDataSettings.ftl"))

    ctx.add_transforms(
        "browser/browser/preferences/translation.ftl",
        "browser/browser/preferences/translation.ftl",
        transforms_from("""
translation-dialog =
  .buttonlabelaccept = { COPY_PATTERN(from_path, "translation-button-close.label") }
  .buttonaccesskeyaccept = { COPY_PATTERN(from_path, "translation-button-close.accesskey") }
""", from_path="browser/browser/preferences/translation.ftl"))

    ctx.add_transforms(
        "browser/browser/preferences/blocklists.ftl",
        "browser/browser/preferences/blocklists.ftl",
        transforms_from("""
blocklist-dialog =
  .buttonlabelaccept = { COPY_PATTERN(from_path, "blocklist-button-ok.label") }
  .buttonaccesskeyaccept = { COPY_PATTERN(from_path, "blocklist-button-ok.accesskey") }
""", from_path="browser/browser/preferences/blocklists.ftl"))
