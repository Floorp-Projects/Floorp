# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY


def migrate(ctx):
    """Bug 1586246 - Convert autohide-context to Fluent, part {index}"""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
full-screen-autohide =
    .label = { COPY(path1, "fullScreenAutohide.label") }
    .accesskey = { COPY(path1, "fullScreenAutohide.accesskey") }
full-screen-exit =
    .label = { COPY(path1, "fullScreenExit.label") }
    .accesskey = { COPY(path1, "fullScreenExit.accesskey") }
""", path1="browser/chrome/browser/browser.dtd"))
