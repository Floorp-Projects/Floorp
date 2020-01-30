# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, MESSAGE_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY_PATTERN, REPLACE, COPY


def migrate(ctx):
    """Bug 1608022 - Migrate browser-sets to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        [
            FTL.Message(
                id=FTL.Identifier("urlbar-star-edit-bookmark"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOn.tooltip2",
                            {
                                "%1$S": VARIABLE_REFERENCE("shortcut")
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("urlbar-star-add-bookmark"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOff.tooltip2",
                            {
                                "%1$S": VARIABLE_REFERENCE("shortcut")
                            },
                            normalize_printf=True
                        )
                    )
                ]
            ),
        ]
    )

    ctx.add_transforms(
        'browser/browser/browserContext.ftl',
        'browser/browser/browserContext.ftl',
        [
            FTL.Message(
                id=FTL.Identifier("main-context-menu-bookmark-add"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.aria-label",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOff.tooltip2",
                            {
                                " (%1$S)": FTL.TextElement("")
                            },
                            trim=True,
                            normalize_printf=True
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("main-context-menu-bookmark-add-with-shortcut"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.aria-label",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOff.tooltip2",
                            {
                                "%1$S": VARIABLE_REFERENCE("shortcut")
                            },
                            normalize_printf=True
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("main-context-menu-bookmark-change"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "editThisBookmarkCmd.label",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOn.tooltip2",
                            {
                                " (%1$S)": FTL.TextElement("")
                            },
                            trim=True,
                            normalize_printf=True
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("main-context-menu-bookmark-change-with-shortcut"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "editThisBookmarkCmd.label",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            "browser/browser/browserContext.ftl",
                            "main-context-menu-bookmark-page.accesskey",
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "starButtonOn.tooltip2",
                            {
                                "%1$S": VARIABLE_REFERENCE("shortcut")
                            },
                            normalize_printf=True
                        )
                    ),
                ]
            ),
        ]
    )

    ctx.add_transforms(
        'browser/browser/menubar.ftl',
        'browser/browser/menubar.ftl',
        transforms_from(
"""
menu-bookmark-this-page =
    .label = { COPY(from_path, "bookmarkThisPageCmd.label") }
menu-bookmark-edit =
    .label = { COPY(from_path, "editThisBookmarkCmd.label") }
""", from_path="browser/chrome/browser/browser.dtd")
    )
