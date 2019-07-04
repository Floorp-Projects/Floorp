# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import REPLACE, REPLACE_IN_TEXT, PLURALS

TARGET_FILE = 'browser/browser/preferences/preferences.ftl'
SOURCE_FILE = 'browser/browser/preferences/preferences.ftl'

"""
From mozilla-central directory:
```
cd ..
git clone hg::https://hg.mozilla.org/l10n/fluent-migration
cd fluent-migration
pip install -e .
cd ..
hg clone https://hg.mozilla.org/l10n/gecko-strings
```
NB: gecko-strings needs to be cloned with mercurial not git-cinnabar
Testing from mozilla-central directory:
```
PYTHONPATH=./python/l10n/fluent_migrations migrate-l10n bug_1555788_newtab --lang en-US --reference-dir . \
  --localization-dir ../gecko-strings
diff -B -w browser/locales/en-US/browser/preferences/preferences.ftl ../gecko-strings/browser/browser/preferences/preferences.ftl
```
"""

def migrate(ctx):
    """Bug 1555788 - Migrate about preferences home content to use fluent, part {index}"""
    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from(
"""
home-prefs-content-header = { COPY(from_path, "prefs_home_header") }
home-prefs-content-description = { COPY(from_path, "prefs_home_description") }
home-prefs-content-discovery-description = { COPY(from_path, "prefs_content_discovery_description") }

home-prefs-search-header =
    .label = { COPY(from_path, "prefs_search_header") }

home-prefs-highlights-header =
    .label = { COPY(from_path, "settings_pane_highlights_header") }
home-prefs-highlights-description = { COPY(from_path, "prefs_highlights_description") }
home-prefs-highlights-option-visited-pages =
    .label = { COPY(from_path, "prefs_highlights_options_visited_label") }
home-prefs-highlights-option-most-recent-download =
    .label = { COPY(from_path, "prefs_highlights_options_download_label") }
home-prefs-highlights-options-bookmarks =
    .label = { COPY(from_path, "settings_pane_highlights_options_bookmarks") }
home-prefs-snippets-header =
    .label = { COPY(from_path, "settings_pane_snippets_header") }

home-prefs-topsites-description = { COPY(from_path, "prefs_topsites_description") }
home-prefs-topsites-header =
    .label = { COPY(from_path, "settings_pane_topsites_header") }

home-prefs-recommended-by-description = { COPY(from_path, "prefs_topstories_description2") }
home-prefs-recommended-by-learn-more = { COPY(from_path, "pocket_how_it_works") }
home-prefs-recommended-by-option-sponsored-stories =
    .label = { COPY(from_path, "prefs_topstories_options_sponsored_label") }

""", from_path="browser/chrome/browser/activity-stream/newtab.properties"
        )
    )

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        [
            FTL.Message(
                id=FTL.Identifier("home-prefs-recommended-by-header"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/activity-stream/newtab.properties",
                            "header_recommended_by",
                            {
                                "{provider}": VARIABLE_REFERENCE("provider")
                            }
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("home-prefs-highlights-option-saved-to-pocket"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/activity-stream/newtab.properties",
                            "prefs_highlights_options_pocket_label",
                            {
                                "Pocket": TERM_REFERENCE("pocket-brand-name")
                            },
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("home-prefs-snippets-description"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "prefs_snippets_description",
                    {
                        "Mozilla": TERM_REFERENCE("vendor-short-name"),
                        "Firefox": TERM_REFERENCE("brand-product-name")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("home-prefs-sections-rows-option"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            "browser/chrome/browser/activity-stream/newtab.properties",
                            "prefs_section_rows_option",
                            VARIABLE_REFERENCE("num"),
                            lambda text: REPLACE_IN_TEXT(
                                text,
                                {
                                    "{num}": VARIABLE_REFERENCE("num")
                                }
                            )
                        )
                    )
                ]
            )
        ]
    )
