# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import (
    COPY,
    COPY_PATTERN,
    PLURALS,
    REPLACE,
    REPLACE_IN_TEXT,
)


def migrate(ctx):
    """Bug 1793572 - Convert WebRTC strings to Fluent, part {index}."""

    source = "browser/chrome/browser/webrtcIndicator.properties"
    target = "browser/browser/webrtcIndicator.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-window"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY_PATTERN(target, "webrtc-indicator-title"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-sharing-camera-and-microphone"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(
                            source, "webrtcIndicator.sharingCameraAndMicrophone.tooltip"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-sharing-camera"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(source, "webrtcIndicator.sharingCamera.tooltip"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-sharing-microphone"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(source, "webrtcIndicator.sharingMicrophone.tooltip"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-sharing-application"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(
                            source, "webrtcIndicator.sharingApplication.tooltip"
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-sharing-screen"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(source, "webrtcIndicator.sharingScreen.tooltip"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-sharing-window"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(source, "webrtcIndicator.sharingWindow.tooltip"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-sharing-browser"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(source, "webrtcIndicator.sharingBrowser.tooltip"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-menuitem-control-sharing"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "webrtcIndicator.controlSharing.menuitem"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-menuitem-control-sharing-on"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "webrtcIndicator.controlSharingOn.menuitem",
                            {"%1$S": VARIABLE_REFERENCE("streamTitle")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-menuitem-sharing-camera-with"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "webrtcIndicator.sharingCameraWith.menuitem",
                            {"%1$S": VARIABLE_REFERENCE("streamTitle")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-menuitem-sharing-microphone-with"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "webrtcIndicator.sharingMicrophoneWith.menuitem",
                            {"%1$S": VARIABLE_REFERENCE("streamTitle")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-menuitem-sharing-application-with"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "webrtcIndicator.sharingApplicationWith.menuitem",
                            {"%1$S": VARIABLE_REFERENCE("streamTitle")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-menuitem-sharing-screen-with"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "webrtcIndicator.sharingScreenWith.menuitem",
                            {"%1$S": VARIABLE_REFERENCE("streamTitle")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-menuitem-sharing-window-with"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "webrtcIndicator.sharingWindowWith.menuitem",
                            {"%1$S": VARIABLE_REFERENCE("streamTitle")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-indicator-menuitem-sharing-browser-with"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "webrtcIndicator.sharingBrowserWith.menuitem",
                            {"%1$S": VARIABLE_REFERENCE("streamTitle")},
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "webrtc-indicator-menuitem-sharing-camera-with-n-tabs"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "webrtcIndicator.sharingCameraWithNTabs.menuitem",
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
                id=FTL.Identifier(
                    "webrtc-indicator-menuitem-sharing-microphone-with-n-tabs"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "webrtcIndicator.sharingMicrophoneWithNTabs.menuitem",
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
                id=FTL.Identifier(
                    "webrtc-indicator-menuitem-sharing-application-with-n-tabs"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "webrtcIndicator.sharingApplicationWithNTabs.menuitem",
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
                id=FTL.Identifier(
                    "webrtc-indicator-menuitem-sharing-screen-with-n-tabs"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "webrtcIndicator.sharingScreenWithNTabs.menuitem",
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
                id=FTL.Identifier(
                    "webrtc-indicator-menuitem-sharing-window-with-n-tabs"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "webrtcIndicator.sharingWindowWithNTabs.menuitem",
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
                id=FTL.Identifier(
                    "webrtc-indicator-menuitem-sharing-browser-with-n-tabs"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=PLURALS(
                            source,
                            "webrtcIndicator.sharingBrowserWithNTabs.menuitem",
                            VARIABLE_REFERENCE("tabCount"),
                            foreach=lambda n: REPLACE_IN_TEXT(
                                n,
                                {"#1": VARIABLE_REFERENCE("tabCount")},
                            ),
                        ),
                    )
                ],
            ),
        ],
    )
