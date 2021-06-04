# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE
from fluent.migrate import REPLACE


def migrate(ctx):
    """Bug 1691762 - Moving save to Pocket button panel string into fluent, part {index}."""
    ctx.add_transforms(
        "browser/browser/aboutPocket.ftl",
        "browser/browser/aboutPocket.ftl",
        transforms_from(
            """
pocket-panel-saved-add-tags =
    .placeholder = { COPY(from_path, "addtags") }
pocket-panel-saved-error-tag-length = { COPY(from_path, "maxtaglength") }
pocket-panel-saved-error-only-links = { COPY(from_path, "onlylinkssaved") }
pocket-panel-saved-error-not-saved = { COPY(from_path, "pagenotsaved") }
pocket-panel-saved-page-removed = { COPY(from_path, "pageremoved") }
pocket-panel-saved-processing-remove = { COPY(from_path, "processingremove") }
pocket-panel-saved-processing-tags = { COPY(from_path, "processingtags") }
pocket-panel-saved-remove-page = { COPY(from_path, "removepage") }
pocket-panel-saved-save-tags = { COPY(from_path, "save") }
pocket-panel-saved-saving-tags = { COPY(from_path, "saving") }
pocket-panel-saved-suggested-tags = { COPY(from_path, "suggestedtags") }
pocket-panel-saved-tags-saved = { COPY(from_path, "tagssaved") }
pocket-panel-signup-view-list = { COPY(from_path, "viewlist") }
pocket-panel-signup-learn-more = { COPY(from_path, "learnmore") }
pocket-panel-signup-login = { COPY(from_path, "loginnow") }
pocket-panel-signup-signup-email = { COPY(from_path, "signupemail") }
pocket-panel-home-my-list = { COPY(from_path, "mylist") }
pocket-panel-home-welcome-back = { COPY(from_path, "welcomeback") }
pocket-panel-home-explore-popular-topics = { COPY(from_path, "explorepopulartopics") }
pocket-panel-home-discover-more = { COPY(from_path, "discovermore") }
pocket-panel-home-explore-more = { COPY(from_path, "exploremore") }
""",
            from_path="browser/chrome/browser/pocket.properties",
        ),
    )
    ctx.add_transforms(
        "browser/browser/aboutPocket.ftl",
        "browser/browser/aboutPocket.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("pocket-panel-saved-error-generic"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "errorgeneric",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pocket-panel-saved-page-saved"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "pagesaved",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pocket-panel-signup-already-have"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "alreadyhaveacct",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pocket-panel-signup-signup-cta"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "signuptosave",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pocket-panel-signup-signup-firefox"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "signupfirefox",
                    {
                        "Firefox": TERM_REFERENCE("brand-product-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pocket-panel-signup-tagline"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "tagline",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name"),
                        "Firefox": TERM_REFERENCE("brand-product-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pocket-panel-signup-tagline-story-one"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "taglinestory_one",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name"),
                        "Firefox": TERM_REFERENCE("brand-product-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pocket-panel-signup-tagline-story-two"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "taglinestory_two",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pocket-panel-home-paragraph"),
                value=REPLACE(
                    "browser/chrome/browser/pocket.properties",
                    "pockethomeparagraph",
                    {
                        "%1$S": TERM_REFERENCE("pocket-brand-name"),
                    },
                ),
            ),
        ],
    )
