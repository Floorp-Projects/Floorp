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
