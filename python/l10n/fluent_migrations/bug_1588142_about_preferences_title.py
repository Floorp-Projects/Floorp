# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1588142 - about:preferences - migrate the root xul:window element to an html:html element, part {index}."""

    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        transforms_from(
"""
pref-page-title = {COPY_PATTERN(from_file, "pref-page.title")}
""", from_file="browser/browser/preferences/preferences.ftl"))
