# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY
from fluent.migrate import CONCAT
from fluent.migrate import REPLACE
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import VARIABLE_REFERENCE

def migrate(ctx):
    """Bug 1507301 - Migrate about:sessionrestore and about:welcomeback to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "browser/browser/aboutSessionRestore.ftl",
        "browser/browser/aboutSessionRestore.ftl",
        transforms_from(
"""
restore-page-tab-title = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.tabtitle") }
restore-page-error-title = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.errorTitle2") }
restore-page-problem-desc = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.problemDesc2") }
restore-page-try-this = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.tryThis2") }
restore-page-hide-tabs = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.hideTabs") }
restore-page-show-tabs = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.showTabs") }

restore-page-restore-header =
    .label = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.restoreHeader") }
restore-page-list-header =
    .label = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.listHeader") }

restore-page-try-again-button =
    .label = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.tryagainButton2") }
    .accesskey = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.restore.access2") }

restore-page-close-button =
    .label = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.closeButton2") }
    .accesskey = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "restorepage.close.access2") }

welcome-back-tab-title = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "welcomeback2.tabtitle") }
welcome-back-page-title = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "welcomeback2.pageTitle") }

welcome-back-restore-button =
    .label = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "welcomeback2.restoreButton") }
    .accesskey = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "welcomeback2.restoreButton.access") }

welcome-back-restore-all-label = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "welcomeback2.restoreAll.label") }
welcome-back-restore-some-label = { COPY("browser/chrome/browser/aboutSessionRestore.dtd", "welcomeback2.restoreSome.label") }
""")
)


    ctx.add_transforms(
        "browser/browser/aboutSessionRestore.ftl",
        "browser/browser/aboutSessionRestore.ftl",
        [
        FTL.Message(
            id=FTL.Identifier("welcome-back-page-info"),
            value=REPLACE(
                "browser/chrome/browser/aboutSessionRestore.dtd",
                "welcomeback2.pageInfo1",
                {
                    "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                },
            )
        ),

        FTL.Message(
            id=FTL.Identifier("welcome-back-page-info-link"),
            value=CONCAT(
                COPY(
                    "browser/chrome/browser/aboutSessionRestore.dtd",
                    "welcomeback2.beforelink.pageInfo2"
                ),
                FTL.TextElement('<a data-l10n-name="link-more">'),
                COPY(
                    "browser/chrome/browser/aboutSessionRestore.dtd",
                    "welcomeback2.link.pageInfo2"
                ),
                FTL.TextElement('</a>'),
                COPY(
                    "browser/chrome/browser/aboutSessionRestore.dtd",
                    "welcomeback2.afterlink.pageInfo2"
                ),
            )
        ),

        FTL.Message(
            id=FTL.Identifier("restore-page-window-label"),
            value=REPLACE(
                "browser/chrome/browser/aboutSessionRestore.dtd",
                "restorepage.windowLabel",
                {
                    "%S": VARIABLE_REFERENCE("windowNumber"),
                },
            )
        )
    ]
)

