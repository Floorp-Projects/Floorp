# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1760029 - Migrate tabbrowser to Fluent, part {index}."""

    source = "browser/chrome/browser/tabbrowser.properties"
    target = "browser/browser/tabbrowser.ftl"

    ctx.add_transforms(
        target,
        target,
        [
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
        ],
    )
