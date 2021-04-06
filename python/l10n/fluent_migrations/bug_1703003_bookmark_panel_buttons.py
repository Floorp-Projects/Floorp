# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
import fluent.syntax.ast as FTL
from fluent.migrate import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT
from fluent.migrate.helpers import COPY, VARIABLE_REFERENCE


def migrate(ctx):
    """Bug 1703003 - Migrate remove/cancel button in Bookmark panel to Fluent - part {index}"""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """
bookmark-panel-cancel =
    .label = { COPY(from_path, "editBookmarkPanel.cancel.label") }
    .accesskey = { COPY(from_path, "editBookmarkPanel.cancel.accesskey") }
""",
            from_path="browser/chrome/browser/browser.properties",
        ),
    )

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("bookmark-panel-remove"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            "browser/chrome/browser/browser.properties",
                            "editBookmark.removeBookmarks.label",
                            VARIABLE_REFERENCE("count"),
                            lambda text: REPLACE_IN_TEXT(
                                text, {"#1": VARIABLE_REFERENCE("count")}
                            ),
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/browser.properties",
                            "editBookmark.removeBookmarks.accesskey",
                        ),
                    ),
                ],
            )
        ],
    )
