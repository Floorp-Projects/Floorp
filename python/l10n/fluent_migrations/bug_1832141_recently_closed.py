import fluent.syntax.ast as FTL
from fluent.migrate import COPY_PATTERN, PLURALS, REPLACE_IN_TEXT
from fluent.migrate.helpers import VARIABLE_REFERENCE

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


class CUSTOM_PLURALS(PLURALS):
    def __call__(self, ctx):
        pattern = super().__call__(ctx)
        el = pattern.elements[0]
        if isinstance(el, FTL.Placeable) and isinstance(
            el.expression, FTL.SelectExpression
        ):
            selexp = el.expression
        else:
            selexp = FTL.SelectExpression(
                VARIABLE_REFERENCE("tabCount"),
                [FTL.Variant(FTL.Identifier("other"), pattern, default=True)],
            )
            pattern = FTL.Pattern([FTL.Placeable(selexp)])
        selexp.variants[0:0] = [
            FTL.Variant(
                FTL.NumberLiteral("0"),
                FTL.Pattern([FTL.Placeable(VARIABLE_REFERENCE("winTitle"))]),
            )
        ]
        return pattern


def migrate(ctx):
    """Bug 1832141 - Migrate strings to recentlyClosed.ftl, part {index}."""

    appmenu = "browser/browser/appmenu.ftl"
    browser = "browser/chrome/browser/browser.properties"
    menubar = "browser/browser/menubar.ftl"
    target = "browser/browser/recentlyClosed.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("recently-closed-menu-reopen-all-tabs"),
                value=COPY_PATTERN(menubar, "menu-history-reopen-all-tabs"),
            ),
            FTL.Message(
                id=FTL.Identifier("recently-closed-menu-reopen-all-windows"),
                value=COPY_PATTERN(menubar, "menu-history-reopen-all-windows"),
            ),
            FTL.Message(
                id=FTL.Identifier("recently-closed-panel-reopen-all-tabs"),
                value=COPY_PATTERN(appmenu, "appmenu-reopen-all-tabs"),
            ),
            FTL.Message(
                id=FTL.Identifier("recently-closed-panel-reopen-all-windows"),
                value=COPY_PATTERN(appmenu, "appmenu-reopen-all-windows"),
            ),
            FTL.Message(
                id=FTL.Identifier("recently-closed-undo-close-window-label"),
                value=CUSTOM_PLURALS(
                    browser,
                    "menuUndoCloseWindowLabel",
                    VARIABLE_REFERENCE("tabCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": VARIABLE_REFERENCE("winTitle"),
                            "#2": VARIABLE_REFERENCE("tabCount"),
                        },
                    ),
                ),
            ),
        ],
    )
