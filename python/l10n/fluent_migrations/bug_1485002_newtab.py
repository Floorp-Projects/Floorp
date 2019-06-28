# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, REPLACE

TARGET_FILE = 'browser/browser/newtab/newtab.ftl'
SOURCE_FILE = TARGET_FILE

"""
For now while we're testing, use a recipe with slightly different paths for
testing from activity-stream instead of the usual steps:
https://firefox-source-docs.mozilla.org/intl/l10n/l10n/fluent_migrations.html#how-to-test-migration-recipes


One-time setup starting from activity-stream directory:

```
cd ..
git clone hg::https://hg.mozilla.org/l10n/fluent-migration
cd fluent-migration
pip install -e .

cd ..
hg clone https://hg.mozilla.org/l10n/gecko-strings
```
NB: gecko-strings needs to be cloned with mercurial not git-cinnabar


Testing from activity-stream directory:

```
rm -f ../gecko-strings/browser/browser/newtab/newtab.ftl
PYTHONPATH=./bin migrate-l10n bug_1485002_newtab --lang en-US --reference-dir . \
  --localization-dir ../gecko-strings
diff -B locales-src/newtab.ftl ../gecko-strings/browser/browser/newtab/newtab.ftl
```
NB: migrate-l10n will make local commits to gecko-strings

The diff should result in no differences if the migration recipe matches the
fluent file.


NB: Move the following line out of this comment to test from activity-stream
SOURCE_FILE = 'locales-src/newtab.ftl'
"""


def migrate(ctx):
    """Bug 1485002 - Migrate newtab.properties to newtab.ftl, part {index}"""

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        transforms_from("""

newtab-page-title = { COPY(from_path, "newtab_page_title") }
newtab-settings-button =
    .title = { COPY(from_path, "settings_pane_button_label") }

newtab-search-box-search-button =
    .title = { COPY(from_path, "search_button") }
    .aria-label = { COPY(from_path, "search_button") }

newtab-search-box-search-the-web-text = { COPY(from_path, "search_web_placeholder") }
newtab-search-box-search-the-web-input =
    .placeholder = { COPY(from_path, "search_web_placeholder") }
    .title = { COPY(from_path, "search_web_placeholder") }
    .aria-label = { COPY(from_path, "search_web_placeholder") }

newtab-topsites-add-search-engine-header =
    { COPY(from_path, "section_menu_action_add_search_engine") }
newtab-topsites-add-topsites-header = { COPY(from_path, "topsites_form_add_header") }
newtab-topsites-edit-topsites-header = { COPY(from_path, "topsites_form_edit_header") }
newtab-topsites-title-label = { COPY(from_path, "topsites_form_title_label") }
newtab-topsites-title-input =
    .placeholder = { COPY(from_path, "topsites_form_title_placeholder") }

newtab-topsites-url-label = { COPY(from_path, "topsites_form_url_label") }
newtab-topsites-url-input =
    .placeholder = { COPY(from_path, "topsites_form_url_placeholder") }
newtab-topsites-url-validation = { COPY(from_path, "topsites_form_url_validation") }

newtab-topsites-image-url-label = { COPY(from_path, "topsites_form_image_url_label") }
newtab-topsites-use-image-link = { COPY(from_path, "topsites_form_use_image_link") }
newtab-topsites-image-validation = { COPY(from_path, "topsites_form_image_validation") }

newtab-topsites-cancel-button = { COPY(from_path, "topsites_form_cancel_button") }
newtab-topsites-delete-history-button = { COPY(from_path, "menu_action_delete") }
newtab-topsites-save-button = { COPY(from_path, "topsites_form_save_button") }
newtab-topsites-preview-button = { COPY(from_path, "topsites_form_preview_button") }
newtab-topsites-add-button = { COPY(from_path, "topsites_form_add_button") }

newtab-confirm-delete-history-p1 = { COPY(from_path, "confirm_history_delete_p1") }
newtab-confirm-delete-history-p2 = { COPY(from_path, "confirm_history_delete_notice_p2") }

newtab-menu-section-tooltip =
    .title = { COPY(from_path, "context_menu_title") }
    .aria-label = { COPY(from_path, "context_menu_title") }

newtab-menu-topsites-placeholder-tooltip =
    .title = { COPY(from_path, "edit_topsites_edit_button") }
    .aria-label = { COPY(from_path, "edit_topsites_edit_button") }
newtab-menu-edit-topsites = { COPY(from_path, "edit_topsites_button_text") }
newtab-menu-open-new-window = { COPY(from_path, "menu_action_open_new_window") }
newtab-menu-open-new-private-window = { COPY(from_path, "menu_action_open_private_window") }
newtab-menu-dismiss = { COPY(from_path, "menu_action_dismiss") }
newtab-menu-pin = { COPY(from_path, "menu_action_pin") }
newtab-menu-unpin = { COPY(from_path, "menu_action_unpin") }
newtab-menu-delete-history = { COPY(from_path, "menu_action_delete") }
newtab-menu-remove-bookmark = { COPY(from_path, "menu_action_remove_bookmark") }
newtab-menu-bookmark = { COPY(from_path, "menu_action_bookmark") }

newtab-menu-copy-download-link = { COPY(from_path, "menu_action_copy_download_link") }
newtab-menu-go-to-download-page = { COPY(from_path, "menu_action_go_to_download_page") }
newtab-menu-remove-download = { COPY(from_path, "menu_action_remove_download") }


newtab-menu-show-file =
    { PLATFORM() ->
      [macos] { COPY(from_path, "menu_action_show_file_mac_os") }
       *[other] { COPY(from_path, "menu_action_show_file_windows") }
    }
newtab-menu-open-file = { COPY(from_path, "menu_action_open_file") }

newtab-label-visited = { COPY(from_path, "type_label_visited") }
newtab-label-bookmarked = { COPY(from_path, "type_label_bookmarked") }
newtab-label-recommended = { COPY(from_path, "type_label_recommended") }
newtab-label-download = { COPY(from_path, "type_label_downloaded") }

newtab-section-menu-remove-section = { COPY(from_path, "section_menu_action_remove_section") }
newtab-section-menu-collapse-section = { COPY(from_path, "section_menu_action_collapse_section") }
newtab-section-menu-expand-section = { COPY(from_path, "section_menu_action_expand_section") }
newtab-section-menu-manage-section = { COPY(from_path, "section_menu_action_manage_section") }
newtab-section-menu-manage-webext = { COPY(from_path, "section_menu_action_manage_webext") }
newtab-section-menu-add-topsite = { COPY(from_path, "section_menu_action_add_topsite") }
newtab-section-menu-add-search-engine =
    { COPY(from_path, "section_menu_action_add_search_engine") }
newtab-section-menu-move-up = { COPY(from_path, "section_menu_action_move_up") }
newtab-section-menu-move-down = { COPY(from_path, "section_menu_action_move_down") }
newtab-section-menu-privacy-notice = { COPY(from_path, "section_menu_action_privacy_notice") }
newtab-section-header-topsites = { COPY(from_path, "header_top_sites") }
newtab-section-header-highlights = { COPY(from_path, "header_highlights") }
newtab-empty-section-highlights = { COPY(from_path, "highlights_empty_state") }

newtab-pocket-read-more = { COPY(from_path, "pocket_read_more") }
newtab-pocket-more-recommendations = { COPY(from_path, "pocket_more_reccommendations") }
newtab-pocket-how-it-works = { COPY(from_path, "pocket_how_it_works") }

newtab-error-fallback-info = { COPY(from_path, "error_fallback_default_info") }
newtab-error-fallback-refresh-link =
    { COPY(from_path, "error_fallback_default_refresh_suggestion") }

        """, from_path='browser/chrome/browser/activity-stream/newtab.properties')
    )

    ctx.add_transforms(
        TARGET_FILE,
        SOURCE_FILE,
        [
            FTL.Message(
                id=FTL.Identifier("newtab-menu-save-to-pocket"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "menu_action_save_to_pocket",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("newtab-menu-archive-pocket"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "menu_action_archive_pocket",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("newtab-menu-delete-pocket"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "menu_action_delete_pocket",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("newtab-menu-content-tooltip"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(
                            "browser/chrome/browser/activity-stream/newtab.properties",
                            "context_menu_title"
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=REPLACE(
                            "browser/chrome/browser/activity-stream/newtab.properties",
                            "context_menu_button_sr",
                            {
                                "{title}": VARIABLE_REFERENCE("title")
                            },
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("newtab-label-saved"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "type_label_pocket",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("newtab-section-header-pocket"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "header_recommended_by",
                    {
                        "{provider}": VARIABLE_REFERENCE("provider")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("newtab-empty-section-topstories"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "topstories_empty_state",
                    {
                        "{provider}": VARIABLE_REFERENCE("provider")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("newtab-pocket-cta-button"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "pocket_cta_button",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name")
                    },
                )
            ),
            FTL.Message(
                id=FTL.Identifier("newtab-pocket-cta-text"),
                value=REPLACE(
                    "browser/chrome/browser/activity-stream/newtab.properties",
                    "pocket_cta_text",
                    {
                        "Pocket": TERM_REFERENCE("pocket-brand-name")
                    },
                )
            ),
        ]
    )
