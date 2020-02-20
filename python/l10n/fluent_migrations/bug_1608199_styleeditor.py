# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import CONCAT
from fluent.migrate.helpers import COPY, transforms_from

def migrate(ctx):
    """Bug 1608199 - Port devtools/client/styleeditor.dtd to Fluent, part {index}."""

    ctx.add_transforms(
        "devtools/client/styleeditor.ftl",
        "devtools/client/styleeditor.ftl",
    transforms_from(
"""
styleeditor-new-button =
    .tooltiptext = { COPY(from_path, "newButton.tooltip") }
    .accesskey = { COPY(from_path, "newButton.accesskey") }
styleeditor-import-button =
    .tooltiptext = { COPY(from_path, "importButton.tooltip") }
    .accesskey = { COPY(from_path, "importButton.accesskey") }
styleeditor-visibility-toggle =
    .tooltiptext = { COPY(from_path, "visibilityToggle.tooltip")}
    .accesskey = { COPY(from_path, "saveButton.accesskey") }
styleeditor-save-button = { COPY(from_path, "saveButton.label") }
    .tooltiptext = { COPY(from_path, "saveButton.tooltip") }
    .accesskey = { COPY(from_path, "saveButton.accesskey") }
styleeditor-options-button =
    .tooltiptext = { COPY(from_path, "optionsButton.tooltip") }
styleeditor-media-rules = { COPY(from_path, "mediaRules.label") }
styleeditor-editor-textbox =
    .data-placeholder = { COPY(from_path, "editorTextbox.placeholder") }
styleeditor-no-stylesheet = { COPY(from_path, "noStyleSheet.label") }
styleeditor-open-link-new-tab =
    .label = { COPY(from_path, "openLinkNewTab.label") }
styleeditor-copy-url =
    .label = { COPY(from_path, "copyUrl.label") }
""", from_path="devtools/client/styleeditor.dtd"))

    ctx.add_transforms(
        "devtools/client/styleeditor.ftl",
        "devtools/client/styleeditor.ftl",
        [
            FTL.Message(
                        id=FTL.Identifier("styleeditor-no-stylesheet-tip"),
                        value=CONCAT(
                                    COPY(
                                        "devtools/client/styleeditor.dtd",
                                         "noStyleSheet-tip-start.label",
                                         ),
                                    FTL.TextElement('<a data-l10n-name="append-new-stylesheet">'),
                                    COPY(
                                        "devtools/client/styleeditor.dtd",
                                        "noStyleSheet-tip-action.label",
                                        ),
                                    FTL.TextElement("</a>"),
                                    COPY("devtools/client/styleeditor.dtd",
                                         "noStyleSheet-tip-end.label",
                                         ),
                                    ),
            )
        ]
    )
