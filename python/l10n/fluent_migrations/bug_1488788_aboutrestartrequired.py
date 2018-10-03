# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate import REPLACE

def migrate(ctx):
    """Bug 1488788- Migrate about:restartrequired from DTD to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/aboutRestartRequired.ftl",
        "browser/browser/aboutRestartRequired.ftl",
        transforms_from(
"""
restart-required-title = { COPY("browser/chrome/browser/aboutRestartRequired.dtd", "restartRequired.title") }

restart-required-header = { COPY("browser/chrome/browser/aboutRestartRequired.dtd", "restartRequired.header") }
""")
        )

