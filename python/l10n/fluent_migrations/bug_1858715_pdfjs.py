# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1858715 - Convert viewer.properties to Fluent, part {index}."""

    source = "browser/pdfviewer/viewer.properties"
    target = "toolkit/toolkit/pdfviewer/viewer.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("pdfjs-previous-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "previous.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-previous-button-label"),
                value=COPY(source, "previous_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-next-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "next.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-next-button-label"),
                value=COPY(source, "next_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-input"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "page.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-of-pages"),
                value=REPLACE(
                    source,
                    "of_pages",
                    {"{{pagesCount}}": VARIABLE_REFERENCE("pagesCount")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-of-pages"),
                value=REPLACE(
                    source,
                    "page_of_pages",
                    {
                        "{{pageNumber}}": VARIABLE_REFERENCE("pageNumber"),
                        "{{pagesCount}}": VARIABLE_REFERENCE("pagesCount"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-zoom-out-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "zoom_out.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-zoom-out-button-label"),
                value=COPY(source, "zoom_out_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-zoom-in-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "zoom_in.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-zoom-in-button-label"),
                value=COPY(source, "zoom_in_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-zoom-select"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "zoom.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-presentation-mode-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "presentation_mode.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-presentation-mode-button-label"),
                value=COPY(source, "presentation_mode_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-open-file-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "open_file.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-open-file-button-label"),
                value=COPY(source, "open_file_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-print-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "print.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-print-button-label"),
                value=COPY(source, "print_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-save-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "save.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-save-button-label"),
                value=COPY(source, "save_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-download-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "download_button.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-download-button-label"),
                value=COPY(source, "download_button_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-bookmark-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "bookmark1.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-bookmark-button-label"),
                value=COPY(source, "bookmark1_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-open-in-app-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "open_in_app.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-open-in-app-button-label"),
                value=COPY(source, "open_in_app_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-tools-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "tools.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-tools-button-label"),
                value=COPY(source, "tools_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-first-page-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "first_page.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-first-page-button-label"),
                value=COPY(source, "first_page_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-last-page-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "last_page.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-last-page-button-label"),
                value=COPY(source, "last_page_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-rotate-cw-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "page_rotate_cw.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-rotate-cw-button-label"),
                value=COPY(source, "page_rotate_cw_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-rotate-ccw-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "page_rotate_ccw.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-rotate-ccw-button-label"),
                value=COPY(source, "page_rotate_ccw_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-cursor-text-select-tool-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "cursor_text_select_tool.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-cursor-text-select-tool-button-label"),
                value=COPY(source, "cursor_text_select_tool_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-cursor-hand-tool-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "cursor_hand_tool.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-cursor-hand-tool-button-label"),
                value=COPY(source, "cursor_hand_tool_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-scroll-page-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "scroll_page.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-scroll-page-button-label"),
                value=COPY(source, "scroll_page_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-scroll-vertical-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "scroll_vertical.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-scroll-vertical-button-label"),
                value=COPY(source, "scroll_vertical_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-scroll-horizontal-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "scroll_horizontal.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-scroll-horizontal-button-label"),
                value=COPY(source, "scroll_horizontal_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-scroll-wrapped-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "scroll_wrapped.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-scroll-wrapped-button-label"),
                value=COPY(source, "scroll_wrapped_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-spread-none-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "spread_none.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-spread-none-button-label"),
                value=COPY(source, "spread_none_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-spread-odd-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "spread_odd.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-spread-odd-button-label"),
                value=COPY(source, "spread_odd_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-spread-even-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "spread_even.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-spread-even-button-label"),
                value=COPY(source, "spread_even_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "document_properties.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-button-label"),
                value=COPY(source, "document_properties_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-file-name"),
                value=COPY(source, "document_properties_file_name"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-file-size"),
                value=COPY(source, "document_properties_file_size"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-kb"),
                value=REPLACE(
                    source,
                    "document_properties_kb",
                    {
                        "{{size_kb}}": VARIABLE_REFERENCE("size_kb"),
                        "{{size_b}}": VARIABLE_REFERENCE("size_b"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-mb"),
                value=REPLACE(
                    source,
                    "document_properties_mb",
                    {
                        "{{size_mb}}": VARIABLE_REFERENCE("size_mb"),
                        "{{size_b}}": VARIABLE_REFERENCE("size_b"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-title"),
                value=COPY(source, "document_properties_title"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-author"),
                value=COPY(source, "document_properties_author"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-subject"),
                value=COPY(source, "document_properties_subject"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-keywords"),
                value=COPY(source, "document_properties_keywords"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-creation-date"),
                value=COPY(source, "document_properties_creation_date"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-modification-date"),
                value=COPY(source, "document_properties_modification_date"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-date-string"),
                value=REPLACE(
                    source,
                    "document_properties_date_string",
                    {
                        "{{date}}": VARIABLE_REFERENCE("date"),
                        "{{time}}": VARIABLE_REFERENCE("time"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-creator"),
                value=COPY(source, "document_properties_creator"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-producer"),
                value=COPY(source, "document_properties_producer"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-version"),
                value=COPY(source, "document_properties_version"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-page-count"),
                value=COPY(source, "document_properties_page_count"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-page-size"),
                value=COPY(source, "document_properties_page_size"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-page-size-unit-inches"),
                value=COPY(source, "document_properties_page_size_unit_inches"),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "pdfjs-document-properties-page-size-unit-millimeters"
                ),
                value=COPY(source, "document_properties_page_size_unit_millimeters"),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "pdfjs-document-properties-page-size-orientation-portrait"
                ),
                value=COPY(
                    source, "document_properties_page_size_orientation_portrait"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "pdfjs-document-properties-page-size-orientation-landscape"
                ),
                value=COPY(
                    source, "document_properties_page_size_orientation_landscape"
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-page-size-name-a-three"),
                value=COPY(source, "document_properties_page_size_name_a3"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-page-size-name-a-four"),
                value=COPY(source, "document_properties_page_size_name_a4"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-page-size-name-letter"),
                value=COPY(source, "document_properties_page_size_name_letter"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-page-size-name-legal"),
                value=COPY(source, "document_properties_page_size_name_legal"),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "pdfjs-document-properties-page-size-dimension-string"
                ),
                value=REPLACE(
                    source,
                    "document_properties_page_size_dimension_string",
                    {
                        "{{width}}": VARIABLE_REFERENCE("width"),
                        "{{height}}": VARIABLE_REFERENCE("height"),
                        "{{unit}}": VARIABLE_REFERENCE("unit"),
                        "{{orientation}}": VARIABLE_REFERENCE("orientation"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "pdfjs-document-properties-page-size-dimension-name-string"
                ),
                value=REPLACE(
                    source,
                    "document_properties_page_size_dimension_name_string",
                    {
                        "{{width}}": VARIABLE_REFERENCE("width"),
                        "{{height}}": VARIABLE_REFERENCE("height"),
                        "{{unit}}": VARIABLE_REFERENCE("unit"),
                        "{{name}}": VARIABLE_REFERENCE("name"),
                        "{{orientation}}": VARIABLE_REFERENCE("orientation"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-linearized"),
                value=COPY(source, "document_properties_linearized"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-linearized-yes"),
                value=COPY(source, "document_properties_linearized_yes"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-linearized-no"),
                value=COPY(source, "document_properties_linearized_no"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-properties-close-button"),
                value=COPY(source, "document_properties_close"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-print-progress-message"),
                value=COPY(source, "print_progress_message"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-print-progress-percent"),
                value=REPLACE(
                    source,
                    "print_progress_percent",
                    {
                        "{{progress}}": VARIABLE_REFERENCE("progress"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-print-progress-close-button"),
                value=COPY(source, "print_progress_close"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-toggle-sidebar-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "toggle_sidebar.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-toggle-sidebar-notification-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "toggle_sidebar_notification2.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-toggle-sidebar-button-label"),
                value=COPY(source, "toggle_sidebar_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-outline-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "document_outline.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-document-outline-button-label"),
                value=COPY(source, "document_outline_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-attachments-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "attachments.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-attachments-button-label"),
                value=COPY(source, "attachments_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-layers-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "layers.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-layers-button-label"),
                value=COPY(source, "layers_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-thumbs-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "thumbs.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-thumbs-button-label"),
                value=COPY(source, "thumbs_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-current-outline-item-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "current_outline_item.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-current-outline-item-button-label"),
                value=COPY(source, "current_outline_item_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-findbar-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"), value=COPY(source, "findbar.title")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-findbar-button-label"),
                value=COPY(source, "findbar_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-additional-layers"),
                value=COPY(source, "additional_layers"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-landmark"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=REPLACE(
                            source,
                            "page_landmark",
                            {
                                "{{page}}": VARIABLE_REFERENCE("page"),
                            },
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-thumb-page-title"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=REPLACE(
                            source,
                            "thumb_page_title",
                            {
                                "{{page}}": VARIABLE_REFERENCE("page"),
                            },
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-thumb-page-canvas"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=REPLACE(
                            source,
                            "thumb_page_canvas",
                            {
                                "{{page}}": VARIABLE_REFERENCE("page"),
                            },
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-input"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "find_input.title"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("placeholder"),
                        value=COPY(source, "find_input.placeholder"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-previous-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "find_previous.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-previous-button-label"),
                value=COPY(source, "find_previous_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-next-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "find_next.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-next-button-label"),
                value=COPY(source, "find_next_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-highlight-checkbox"),
                value=COPY(source, "find_highlight"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-match-case-checkbox-label"),
                value=COPY(source, "find_match_case_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-match-diacritics-checkbox-label"),
                value=COPY(source, "find_match_diacritics_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-entire-word-checkbox-label"),
                value=COPY(source, "find_entire_word_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-reached-top"),
                value=COPY(source, "find_reached_top"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-reached-bottom"),
                value=COPY(source, "find_reached_bottom"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-find-not-found"),
                value=COPY(source, "find_not_found"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-scale-width"),
                value=COPY(source, "page_scale_width"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-scale-fit"),
                value=COPY(source, "page_scale_fit"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-scale-auto"),
                value=COPY(source, "page_scale_auto"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-scale-actual"),
                value=COPY(source, "page_scale_actual"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-page-scale-percent"),
                value=REPLACE(
                    source,
                    "page_scale_percent",
                    {
                        "{{scale}}": VARIABLE_REFERENCE("scale"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-loading-error"),
                value=COPY(source, "loading_error"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-invalid-file-error"),
                value=COPY(source, "invalid_file_error"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-missing-file-error"),
                value=COPY(source, "missing_file_error"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-unexpected-response-error"),
                value=COPY(source, "unexpected_response_error"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-rendering-error"),
                value=COPY(source, "rendering_error"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-annotation-date-string"),
                value=REPLACE(
                    source,
                    "annotation_date_string",
                    {
                        "{{date}}": VARIABLE_REFERENCE("date"),
                        "{{time}}": VARIABLE_REFERENCE("time"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-text-annotation-type"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("alt"),
                        value=REPLACE(
                            source,
                            "text_annotation_type.alt",
                            {
                                "{{type}}": VARIABLE_REFERENCE("type"),
                            },
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-password-label"),
                value=COPY(source, "password_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-password-invalid"),
                value=COPY(source, "password_invalid"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-password-ok-button"),
                value=COPY(source, "password_ok"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-password-cancel-button"),
                value=COPY(source, "password_cancel"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-printing-not-supported"),
                value=COPY(source, "printing_not_supported"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-printing-not-ready"),
                value=COPY(source, "printing_not_ready"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-web-fonts-disabled"),
                value=COPY(source, "web_fonts_disabled"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-free-text-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "editor_free_text2.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-free-text-button-label"),
                value=COPY(source, "editor_free_text2_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-ink-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "editor_ink2.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-ink-button-label"),
                value=COPY(source, "editor_ink2_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-stamp-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "editor_stamp1.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-stamp-button-label"),
                value=COPY(source, "editor_stamp1_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-free-text-default-content"),
                value=COPY(source, "free_text2_default_content"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-free-text-color-input"),
                value=COPY(source, "editor_free_text_color"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-free-text-size-input"),
                value=COPY(source, "editor_free_text_size"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-ink-color-input"),
                value=COPY(source, "editor_ink_color"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-ink-thickness-input"),
                value=COPY(source, "editor_ink_thickness"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-ink-opacity-input"),
                value=COPY(source, "editor_ink_opacity"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-stamp-add-image-button-label"),
                value=COPY(source, "editor_stamp_add_image_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-stamp-add-image-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "editor_stamp_add_image.title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-free-text"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(source, "editor_free_text2_aria_label"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-ink"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(source, "editor_ink2_aria_label"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-ink-canvas"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(source, "editor_ink_canvas_aria_label"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-button-label"),
                value=COPY(source, "editor_alt_text_button_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-edit-button-label"),
                value=COPY(source, "editor_alt_text_edit_button_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-dialog-label"),
                value=COPY(source, "editor_alt_text_dialog_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-dialog-description"),
                value=COPY(source, "editor_alt_text_dialog_description"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-add-description-label"),
                value=COPY(source, "editor_alt_text_add_description_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-add-description-description"),
                value=COPY(source, "editor_alt_text_add_description_description"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-mark-decorative-label"),
                value=COPY(source, "editor_alt_text_mark_decorative_label"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-mark-decorative-description"),
                value=COPY(source, "editor_alt_text_mark_decorative_description"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-cancel-button"),
                value=COPY(source, "editor_alt_text_cancel_button"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-save-button"),
                value=COPY(source, "editor_alt_text_save_button"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-decorative-tooltip"),
                value=COPY(source, "editor_alt_text_decorative_tooltip"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-alt-text-textarea"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("placeholder"),
                        value=COPY(source, "editor_alt_text_textarea.placeholder"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-resizer-label-top-left"),
                value=COPY(source, "editor_resizer_label_topLeft"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-resizer-label-top-middle"),
                value=COPY(source, "editor_resizer_label_topMiddle"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-resizer-label-top-right"),
                value=COPY(source, "editor_resizer_label_topRight"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-resizer-label-middle-right"),
                value=COPY(source, "editor_resizer_label_middleRight"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-resizer-label-bottom-right"),
                value=COPY(source, "editor_resizer_label_bottomRight"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-resizer-label-bottom-middle"),
                value=COPY(source, "editor_resizer_label_bottomMiddle"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-resizer-label-bottom-left"),
                value=COPY(source, "editor_resizer_label_bottomLeft"),
            ),
            FTL.Message(
                id=FTL.Identifier("pdfjs-editor-resizer-label-middle-left"),
                value=COPY(source, "editor_resizer_label_middleLeft"),
            ),
        ],
    )
