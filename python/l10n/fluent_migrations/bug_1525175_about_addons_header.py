
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY_PATTERN, COPY, REPLACE
from fluent.migrate.helpers import TERM_REFERENCE


def migrate(ctx):
    """Bug 1525175 - Convert about:addons header to HTML, part {index}"""

    ctx.add_transforms(
        'toolkit/toolkit/about/aboutAddons.ftl',
        'toolkit/toolkit/about/aboutAddons.ftl',
        transforms_from(
"""
addon-updates-check-for-updates = { COPY_PATTERN(from_path, "extensions-updates-check-for-updates.label") }
    .accesskey = { COPY_PATTERN(from_path, "extensions-updates-check-for-updates.accesskey") }
addon-updates-view-updates = { COPY_PATTERN(from_path, "extensions-updates-view-updates.label") }
    .accesskey = { COPY_PATTERN(from_path, "extensions-updates-view-updates.accesskey") }
addon-updates-update-addons-automatically = { COPY_PATTERN(from_path, "extensions-updates-update-addons-automatically.label") }
    .accesskey = { COPY_PATTERN(from_path, "extensions-updates-update-addons-automatically.accesskey") }
addon-updates-reset-updates-to-automatic = { COPY_PATTERN(from_path, "extensions-updates-reset-updates-to-automatic.label") }
    .accesskey = { COPY_PATTERN(from_path, "extensions-updates-reset-updates-to-automatic.accesskey") }
addon-updates-reset-updates-to-manual = { COPY_PATTERN(from_path, "extensions-updates-reset-updates-to-manual.label") }
    .accesskey = { COPY_PATTERN(from_path, "extensions-updates-reset-updates-to-manual.accesskey") }
addon-updates-updating = { COPY_PATTERN(from_path, "extensions-updates-updating.value") }
addon-updates-installed = { COPY_PATTERN(from_path, "extensions-updates-installed.value") }
addon-updates-none-found = { COPY_PATTERN(from_path, "extensions-updates-none-found.value") }
addon-updates-manual-updates-found = { COPY_PATTERN(from_path, "extensions-updates-manual-updates-found.label") }
addon-install-from-file = { COPY_PATTERN(from_path, "install-addon-from-file.label") }
    .accesskey = { COPY_PATTERN(from_path, "install-addon-from-file.accesskey") }
addon-open-about-debugging = { COPY_PATTERN(from_path, "debug-addons.label") }
    .accesskey = { COPY_PATTERN(from_path, "debug-addons.accesskey") }
addon-manage-extensions-shortcuts = { COPY_PATTERN(from_path, "manage-extensions-shortcuts.label") }
    .accesskey = { COPY_PATTERN(from_path, "manage-extensions-shortcuts.accesskey") }
addons-heading-search-input =
    .placeholder = { COPY_PATTERN(from_path, "search-header.placeholder") }
"""
        , from_path='toolkit/toolkit/about/aboutAddons.ftl')
    )

    ctx.add_transforms(
        'toolkit/toolkit/about/aboutAddons.ftl',
        'toolkit/toolkit/about/aboutAddons.ftl',
        transforms_from(
"""
addon-install-from-file-dialog-title = { COPY(from_path, "installFromFile.dialogTitle") }
addon-install-from-file-filter-name = { COPY(from_path, "installFromFile.filterName") }
extension-heading = { COPY(from_path, "listHeading.extension") }
theme-heading = { COPY(from_path, "listHeading.theme") }
plugin-heading = { COPY(from_path, "listHeading.plugin") }
dictionary-heading = { COPY(from_path, "listHeading.dictionary") }
locale-heading = { COPY(from_path, "listHeading.locale") }
shortcuts-heading = { COPY(from_path, "listHeading.shortcuts") }
theme-heading-search-label = { COPY(from_path, "searchLabel.theme") }
extension-heading-search-label = { COPY(from_path, "searchLabel.extension") }
"""
        , from_path='toolkit/chrome/mozapps/extensions/extensions.properties')
    )

    ctx.add_transforms(
        'toolkit/toolkit/about/aboutAddons.ftl',
        'toolkit/toolkit/about/aboutAddons.ftl',
        [
            FTL.Message(
                id=FTL.Identifier("discover-heading"),
                value=REPLACE(
                    "toolkit/chrome/mozapps/extensions/extensions.properties",
                    "listHeading.discover",
                    {
                        "%1$S": TERM_REFERENCE("brand-short-name")
                    },
                    normalize_printf=True
                )
            ),
        ]
    )
