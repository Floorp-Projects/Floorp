# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1786185 - Migrate XMLPrettyPrint from DTD to FTL, part {index}"""

    ctx.add_transforms(
        "dom/dom/XMLPrettyPrint.ftl",
        "dom/dom/XMLPrettyPrint.ftl",
        transforms_from(
            """
xml-nostylesheet = { COPY(path1, "xml.nostylesheet") }
""",
            path1="dom/chrome/xml/prettyprint.dtd",
        ),
    )
