# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY

def migrate(ctx):
    """Bug 1517508 - Migrate about:robots to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "browser/browser/aboutRobots.ftl",
        "browser/browser/aboutRobots.ftl",
        transforms_from(
"""
page-title = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.pagetitle") }
error-title-text = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.errorTitleText") }
error-short-desc-text = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.errorShortDescText") }
error-long-desc1 = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.errorLongDesc1") }
error-long-desc2 = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.errorLongDesc2") }
error-long-desc3 = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.errorLongDesc3") }
error-long-desc4 = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.errorLongDesc4") }
error-trailer-desc-text = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.errorTrailerDescText") }
error-try-again = { COPY("browser/chrome/overrides/netError.dtd", "retry.label") }
    .label2 = { COPY("browser/chrome/browser/aboutRobots.dtd", "robots.dontpress") }
""")
)
