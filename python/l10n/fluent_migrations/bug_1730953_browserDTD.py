# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1730953 - Migrate strings from browser.dtd to Fluent, part {index}"""
    ctx.add_transforms(
        "browser/browser/places.ftl",
        "browser/browser/places.ftl",
        transforms_from(
            """
places-history =
  .aria-label = { COPY(from_path, "historyButton.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )

    ctx.add_transforms(
        "browser/browser/search.ftl",
        "browser/browser/search.ftl",
        transforms_from(
            """
searchbar-submit =
    .tooltiptext = { COPY(from_path, "contentSearchSubmit.tooltip") }

searchbar-input =
    .placeholder = { COPY(from_path, "searchInput.placeholder") }

searchbar-icon =
    .tooltiptext = { COPY(from_path, "searchIcon.tooltip") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
