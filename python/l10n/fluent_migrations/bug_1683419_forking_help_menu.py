# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1683419 - Fork the Help menu strings for use in the AppMenu, part {index}."""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenu-about =
    .label = { COPY_PATTERN(from_path, "menu-about.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-about.accesskey") }
appmenu-help-product =
    .label = { COPY_PATTERN(from_path, "menu-help-product.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-product.accesskey") }
appmenu-help-show-tour =
    .label = { COPY_PATTERN(from_path, "menu-help-show-tour.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-show-tour.accesskey") }
appmenu-help-import-from-another-browser =
    .label = { COPY_PATTERN(from_path, "menu-help-import-from-another-browser.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-import-from-another-browser.accesskey") }
appmenu-help-keyboard-shortcuts =
    .label = { COPY_PATTERN(from_path, "menu-help-keyboard-shortcuts.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-keyboard-shortcuts.accesskey") }
appmenu-help-troubleshooting-info =
    .label = { COPY_PATTERN(from_path, "menu-help-troubleshooting-info.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-troubleshooting-info.accesskey") }
appmenu-help-feedback-page =
    .label = { COPY_PATTERN(from_path, "menu-help-feedback-page.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-feedback-page.accesskey") }
appmenu-help-safe-mode-without-addons =
    .label = { COPY_PATTERN(from_path, "menu-help-safe-mode-without-addons.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-safe-mode-without-addons.accesskey") }
appmenu-help-safe-mode-with-addons =
    .label = { COPY_PATTERN(from_path, "menu-help-safe-mode-with-addons.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-safe-mode-with-addons.accesskey") }
appmenu-help-report-deceptive-site =
    .label = { COPY_PATTERN(from_path, "menu-help-report-deceptive-site.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-report-deceptive-site.accesskey") }
appmenu-help-not-deceptive =
    .label = { COPY_PATTERN(from_path, "menu-help-not-deceptive.label") }
    .accesskey = { COPY_PATTERN(from_path, "menu-help-not-deceptive.accesskey") }
appmenu-help-check-for-update =
    .label = { COPY_PATTERN(from_path, "menu-help-check-for-update.label") }
""",
            from_path="browser/browser/menubar.ftl",
        ),
    )
