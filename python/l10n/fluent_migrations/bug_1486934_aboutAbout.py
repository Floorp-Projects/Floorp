# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1486934 - Modify about:about to use fluent for localization, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutAbout.ftl",
        "toolkit/toolkit/about/aboutAbout.ftl",
        transforms_from(
"""
about-about-title = { COPY("toolkit/chrome/global/aboutAbout.dtd", "aboutAbout.title") }
about-about-note = { TRIM_COPY("toolkit/chrome/global/aboutAbout.dtd", "aboutAbout.note") }
""")
)
