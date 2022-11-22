# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT, Transform


def migrate(ctx):
    """Bug 1760029 - Migrate tabbrowser to Fluent, part {index}."""

    source = "browser/chrome/browser/tabbrowser.properties"
    target = "browser/browser/tabbrowser.ftl"

    browser_source = "browser/chrome/browser/browser.properties"
    browser_target = "browser/browser/browser.ftl"

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("tabbrowser-empty-tab-title"),
                value=COPY(source, "tabs.emptyTabTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-empty-private-tab-title"),
                value=COPY(source, "tabs.emptyPrivateTabTitle2"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-menuitem-close-tab"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"), value=COPY(source, "tabs.closeTab")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-menuitem-close"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"), value=COPY(source, "tabs.close")
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-container-tab-title"),
                value=REPLACE(
                    source,
                    "tabs.containers.tooltip",
                    {
                        "%1$S": VARIABLE_REFERENCE("title"),
                        "%2$S": VARIABLE_REFERENCE("containerName"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-tab-tooltip"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern([FTL.Placeable(VARIABLE_REFERENCE("title"))]),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-close-tabs-tooltip"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "tabs.closeTabs.tooltip",
                            VARIABLE_REFERENCE("tabCount"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {"#1": VARIABLE_REFERENCE("tabCount")},
                            ),
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-mute-tab-audio-tooltip"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "tabs.muteAudio2.tooltip",
                            VARIABLE_REFERENCE("tabCount"),
                            foreach=lambda text: REPLACE_IN_TEXT(
                                text,
                                {
                                    "#1": VARIABLE_REFERENCE("tabCount"),
                                    "%S": VARIABLE_REFERENCE("shortcut"),
                                },
                            ),
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-unmute-tab-audio-tooltip"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "tabs.unmuteAudio2.tooltip",
                            VARIABLE_REFERENCE("tabCount"),
                            foreach=lambda text: REPLACE_IN_TEXT(
                                text,
                                {
                                    "#1": VARIABLE_REFERENCE("tabCount"),
                                    "%S": VARIABLE_REFERENCE("shortcut"),
                                },
                            ),
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-mute-tab-audio-background-tooltip"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "tabs.muteAudio2.background.tooltip",
                            VARIABLE_REFERENCE("tabCount"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {"#1": VARIABLE_REFERENCE("tabCount")},
                            ),
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-unmute-tab-audio-background-tooltip"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "tabs.unmuteAudio2.background.tooltip",
                            VARIABLE_REFERENCE("tabCount"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {"#1": VARIABLE_REFERENCE("tabCount")},
                            ),
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-unblock-tab-audio-tooltip"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "tabs.unblockAudio2.tooltip",
                            VARIABLE_REFERENCE("tabCount"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {"#1": VARIABLE_REFERENCE("tabCount")},
                            ),
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-close-tabs-title"),
                value=PLURALS(
                    source,
                    "tabs.closeTabsTitle",
                    VARIABLE_REFERENCE("tabCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("tabCount")},
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-close-tabs-button"),
                value=COPY(source, "tabs.closeButtonMultiple"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-close-tabs-checkbox"),
                value=COPY(source, "tabs.closeTabsConfirmCheckbox"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-close-windows-title"),
                value=PLURALS(
                    source,
                    "tabs.closeWindowsTitle",
                    VARIABLE_REFERENCE("windowCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("windowCount")},
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-close-windows-button"),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=FTL.FunctionReference(
                            id=FTL.Identifier("PLATFORM"), arguments=FTL.CallArguments()
                        ),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("windows"),
                                value=COPY(source, "tabs.closeWindowsButtonWin"),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                value=COPY(source, "tabs.closeWindowsButton"),
                                default=True,
                            ),
                        ],
                    )
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-close-tabs-with-key-title"),
                value=REPLACE(
                    source,
                    "tabs.closeTabsWithKeyTitle",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-close-tabs-with-key-button"),
                value=REPLACE(
                    source,
                    "tabs.closeTabsWithKeyButton",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-close-tabs-with-key-checkbox"),
                value=REPLACE(
                    source,
                    "tabs.closeTabsWithKeyConfirmCheckbox",
                    {"%1$S": VARIABLE_REFERENCE("quitKey")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-open-multiple-tabs-title"),
                value=COPY(source, "tabs.openWarningTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-open-multiple-tabs-message"),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=VARIABLE_REFERENCE("tabCount"),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                value=REPLACE(
                                    source,
                                    "tabs.openWarningMultipleBranded",
                                    {
                                        "%1$S": VARIABLE_REFERENCE("tabCount"),
                                        "%2$S": TERM_REFERENCE("brand-short-name"),
                                        ".  ": FTL.TextElement(". "),
                                    },
                                ),
                                default=True,
                            ),
                        ],
                    )
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-open-multiple-tabs-button"),
                value=COPY(source, "tabs.openButtonMultiple"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-open-multiple-tabs-checkbox"),
                value=REPLACE(
                    source,
                    "tabs.openWarningPromptMeBranded",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-caretbrowsing-title"),
                value=COPY(source, "browsewithcaret.checkWindowTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-caretbrowsing-message"),
                value=COPY(source, "browsewithcaret.checkLabel"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-confirm-caretbrowsing-checkbox"),
                value=COPY(source, "browsewithcaret.checkMsg"),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-customizemode-tab-title"),
                value=REPLACE(
                    browser_source,
                    "customizeMode.tabTitle",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-context-mute-tab"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_source, "muteTab.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser_source, "muteTab.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-context-unmute-tab"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_source, "unmuteTab.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser_source, "unmuteTab.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-context-mute-selected-tabs"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_source, "muteSelectedTabs2.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser_source, "muteSelectedTabs2.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("tabbrowser-context-unmute-selected-tabs"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_source, "unmuteSelectedTabs2.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser_source, "unmuteSelectedTabs2.accesskey"),
                    ),
                ],
            ),
        ],
    )

    ctx.add_transforms(
        browser_target,
        browser_target,
        [
            FTL.Message(
                id=FTL.Identifier("refresh-blocked-refresh-label"),
                value=REPLACE(
                    browser_source,
                    "refreshBlocked.refreshLabel",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("refresh-blocked-redirect-label"),
                value=REPLACE(
                    browser_source,
                    "refreshBlocked.redirectLabel",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("refresh-blocked-allow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser_source, "refreshBlocked.goButton"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser_source, "refreshBlocked.goButton.accesskey"),
                    ),
                ],
            ),
        ],
    )
