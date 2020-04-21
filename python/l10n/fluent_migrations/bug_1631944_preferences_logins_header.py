# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1631944 - Add Lockwise as a keyword for login and password preferences, part {index}"""

    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        transforms_from("""
pane-privacy-logins-and-passwords-header = {COPY_PATTERN(from_path, "logins-header")}
    .searchkeywords = { -lockwise-brand-short-name }
""", from_path="browser/browser/preferences/preferences.ftl"))
