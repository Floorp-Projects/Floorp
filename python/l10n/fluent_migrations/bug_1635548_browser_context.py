# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1635548 - Migrate browser-context.inc to Fluent, part {index}"""
    target = "toolkit/toolkit/global/textActions.ftl"
    reference = "toolkit/toolkit/global/textActions.ftl"
    ctx.add_transforms(
        target,
        reference,
        transforms_from(
            """
text-action-spell-add-to-dictionary =
    .label = { COPY(from_path, "spellAddToDictionary.label") }
    .accesskey = { COPY(from_path, "spellAddToDictionary.accesskey") }

text-action-spell-undo-add-to-dictionary =
    .label = { COPY(from_path, "spellUndoAddToDictionary.label") }
    .accesskey = { COPY(from_path, "spellUndoAddToDictionary.accesskey") }

text-action-spell-check-toggle =
    .label = { COPY(from_path, "spellCheckToggle.label") }
    .accesskey = { COPY(from_path, "spellCheckToggle.accesskey") }

text-action-spell-dictionaries =
    .label = { COPY(from_path, "spellDictionaries.label") }
    .accesskey = { COPY(from_path, "spellDictionaries.accesskey") }
""",
            from_path="toolkit/chrome/global/textcontext.dtd",
        ),
    )

    target = "toolkit/toolkit/global/textActions.ftl"
    reference = "toolkit/toolkit/global/textActions.ftl"
    ctx.add_transforms(
        target,
        reference,
        transforms_from(
            """
text-action-spell-add-dictionaries =
    .label = { COPY(from_path, "spellAddDictionaries.label") }
    .accesskey = { COPY(from_path, "spellAddDictionaries.accesskey") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )

    target = "browser/browser/browserContext.ftl"
    reference = "browser/browser/browserContext.ftl"
    ctx.add_transforms(
        target,
        reference,
        [
            FTL.Message(
                id=FTL.Identifier("main-context-menu-open-link-in-container-tab"),
                attributes=[
                    FTL.Attribute(
                        FTL.Identifier("label"),
                        REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "userContextOpenLink.label",
                            {"%1$S": VARIABLE_REFERENCE("containerName")},
                        ),
                    ),
                    FTL.Attribute(
                        FTL.Identifier("accesskey"),
                        COPY(
                            "browser/chrome/browser/browser.dtd",
                            "openLinkCmdInTab.accesskey",
                        ),
                    ),
                ],
            )
        ],
    )
