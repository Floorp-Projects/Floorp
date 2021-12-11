# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE
from fluent.migrate import COPY, REPLACE


def migrate(ctx):
    """Bug 1728460  - Migrate appmenu-viewcache.inc.xhtml to Fluent, part {index}."""
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        transforms_from(
            """
appmenuitem-print =
    .label = { COPY(from_path, "printCmd.label") }
appmenuitem-downloads =
    .label = { COPY(from_path, "libraryDownloads.label") }
appmenuitem-history =
    .label = { COPY(from_path, "historyMenu.label") }
appmenuitem-zoom =
    .value = { COPY(from_path, "fullZoom.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
    ctx.add_transforms(
        "browser/browser/sync.ftl",
        "browser/browser/sync.ftl",
        transforms_from(
            """
fxa-menu-send-tab-to-device-description = { COPY(from_path, "fxa.service.sendTab.description") }
fxa-menu-send-tab-to-device-syncnotready =
    .label = { COPY(from_path, "sendToDevice.syncNotReady.label") }
""",
            from_path="browser/chrome/browser/browser.dtd",
        ),
    )
    ctx.add_transforms(
        "browser/browser/appmenu.ftl",
        "browser/browser/appmenu.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("appmenuitem-fxa-sign-in"),
                value=REPLACE(
                    "browser/chrome/browser/browser.dtd",
                    "fxa.menu.signin.label",
                    {
                        "&brandProductName;": TERM_REFERENCE("brand-product-name"),
                    },
                ),
            ),
        ],
    )
