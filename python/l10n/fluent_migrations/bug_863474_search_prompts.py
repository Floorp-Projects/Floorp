# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import REPLACE
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE


def migrate(ctx):
    """Bug 863474 - Migrate search prompts to fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/search.ftl",
        "browser/browser/search.ftl",
        transforms_from("""
opensearch-error-duplicate-title = { COPY(from_path, "error_invalid_engine_title") }
""", from_path="toolkit/chrome/search/search.properties"))

    ctx.add_transforms(
        "browser/browser/search.ftl",
        "browser/browser/search.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("opensearch-error-duplicate-desc"),
                value=REPLACE(
                    "toolkit/chrome/search/search.properties",
                    "error_duplicate_engine_msg",
                    {
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                        "%2$S": VARIABLE_REFERENCE("location-url"),
                    },
                    normalize_printf=True
                )
            )
        ]
    )

    ctx.add_transforms(
        "browser/browser/search.ftl",
        "browser/browser/search.ftl",
        transforms_from("""
opensearch-error-format-title = { COPY(from_path, "error_invalid_format_title") }
""", from_path="toolkit/chrome/search/search.properties"))

    ctx.add_transforms(
        "browser/browser/search.ftl",
        "browser/browser/search.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("opensearch-error-format-desc"),
                value=REPLACE(
                    "toolkit/chrome/search/search.properties",
                    "error_invalid_engine_msg2",
                    {
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                        "%2$S": VARIABLE_REFERENCE("location-url"),
                    },
                    normalize_printf=True
                )
            )
        ]
    )

    ctx.add_transforms(
        "browser/browser/search.ftl",
        "browser/browser/search.ftl",
        transforms_from("""
opensearch-error-download-title = { COPY(from_path, "error_loading_engine_title") }
""", from_path="toolkit/chrome/search/search.properties"))

    ctx.add_transforms(
        "browser/browser/search.ftl",
        "browser/browser/search.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("opensearch-error-download-desc"),
                value=REPLACE(
                    "toolkit/chrome/search/search.properties",
                    "error_loading_engine_msg2",
                    {
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                        "%2$S": VARIABLE_REFERENCE("location-url"),
                        "\n": FTL.TextElement(" "),
                    },
                    normalize_printf=True
                )
            )
        ]
    )
