# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1786186 - Migrate mobile about:config to Fluent, part {index}"""

    target = "mobile/android/mobile/android/aboutConfig.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """

config-toolbar-search =
    .placeholder = { COPY(path1, "toolbar.searchPlaceholder") }
config-new-pref-name =
    .placeholder = { COPY(path1, "newPref.namePlaceholder") }
config-new-pref-value-boolean = { COPY(path1, "newPref.valueBoolean") }
config-new-pref-value-string = { COPY(path1, "newPref.valueString") }
config-new-pref-value-integer = { COPY(path1, "newPref.valueInteger") }
config-new-pref-string =
    .placeholder = { COPY(path1, "newPref.stringPlaceholder") }
config-new-pref-number =
    .placeholder = { COPY(path1, "newPref.numberPlaceholder") }
config-new-pref-cancel-button = { COPY(path1, "newPref.cancelButton") }
config-context-menu-copy-pref-name =
    .label = { COPY(path1, "contextMenu.copyPrefName") }
config-context-menu-copy-pref-value =
    .label = { COPY(path1, "contextMenu.copyPrefValue") }
""",
            path1="mobile/android/chrome/config.dtd",
        ),
    )

    source = "mobile/android/chrome/config.properties"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("config-new-pref-create-button"),
                value=COPY(source, "newPref.createButton"),
            ),
            FTL.Message(
                id=FTL.Identifier("config-new-pref-change-button"),
                value=COPY(source, "newPref.changeButton"),
            ),
            FTL.Message(
                id=FTL.Identifier("config-pref-toggle-button"),
                value=COPY(source, "pref.toggleButton"),
            ),
            FTL.Message(
                id=FTL.Identifier("config-pref-reset-button"),
                value=COPY(source, "pref.resetButton"),
            ),
        ],
    )
