# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
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
    browser = "browser/chrome/browser/browser.properties"
    browser_ftl = "browser/browser/browser.ftl"
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
            FTL.Message(
                id=FTL.Identifier("webrtc-item-camera"),
                value=REPLACE(
                    browser,
                    "getUserMedia.sharingMenuCamera",
                    {
                        "%1$S (": FTL.TextElement(""),
                        "%1$S（": FTL.TextElement(""),
                        ")": FTL.TextElement(""),
                        "）": FTL.TextElement(""),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-item-microphone"),
                value=REPLACE(
                    browser,
                    "getUserMedia.sharingMenuMicrophone",
                    {
                        "%1$S (": FTL.TextElement(""),
                        "%1$S（": FTL.TextElement(""),
                        ")": FTL.TextElement(""),
                        "）": FTL.TextElement(""),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-item-audio-capture"),
                value=REPLACE(
                    browser,
                    "getUserMedia.sharingMenuAudioCapture",
                    {
                        "%1$S (": FTL.TextElement(""),
                        "%1$S（": FTL.TextElement(""),
                        ")": FTL.TextElement(""),
                        "）": FTL.TextElement(""),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-item-application"),
                value=REPLACE(
                    browser,
                    "getUserMedia.sharingMenuApplication",
                    {
                        "%1$S (": FTL.TextElement(""),
                        "%1$S（": FTL.TextElement(""),
                        ")": FTL.TextElement(""),
                        "）": FTL.TextElement(""),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-item-screen"),
                value=REPLACE(
                    browser,
                    "getUserMedia.sharingMenuScreen",
                    {
                        "%1$S (": FTL.TextElement(""),
                        "%1$S（": FTL.TextElement(""),
                        ")": FTL.TextElement(""),
                        "）": FTL.TextElement(""),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-item-window"),
                value=REPLACE(
                    browser,
                    "getUserMedia.sharingMenuWindow",
                    {
                        "%1$S (": FTL.TextElement(""),
                        "%1$S（": FTL.TextElement(""),
                        ")": FTL.TextElement(""),
                        "）": FTL.TextElement(""),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-item-browser"),
                value=REPLACE(
                    browser,
                    "getUserMedia.sharingMenuBrowser",
                    {
                        "%1$S (": FTL.TextElement(""),
                        "%1$S（": FTL.TextElement(""),
                        ")": FTL.TextElement(""),
                        "）": FTL.TextElement(""),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-sharing-menuitem-unknown-host"),
                value=COPY(browser, "getUserMedia.sharingMenuUnknownHost"),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-sharing-menuitem"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            [FTL.TextElement("{ $origin } ({ $itemList })")]
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-sharing-menu"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser, "getUserMedia.sharingMenu.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser, "getUserMedia.sharingMenu.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-camera"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareCamera3.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-microphone"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareMicrophone3.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-screen"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareScreen4.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-camera-and-microphone"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareCameraAndMicrophone3.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-camera-and-audio-capture"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareCameraAndAudioCapture3.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-screen-and-microphone"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareScreenAndMicrophone4.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-screen-and-audio-capture"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareScreenAndAudioCapture4.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-audio-capture"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareAudioCapture3.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-speaker"),
                value=REPLACE(
                    browser,
                    "selectAudioOutput.shareSpeaker.message",
                    {"%1$S": VARIABLE_REFERENCE("origin")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-camera-unsafe-delegation"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareCameraUnsafeDelegation2.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("origin"),
                        "%2$S": VARIABLE_REFERENCE("thirdParty"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-microphone-unsafe-delegations"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareMicrophoneUnsafeDelegations2.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("origin"),
                        "%2$S": VARIABLE_REFERENCE("thirdParty"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-screen-unsafe-delegation"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareScreenUnsafeDelegation2.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("origin"),
                        "%2$S": VARIABLE_REFERENCE("thirdParty"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "webrtc-allow-share-camera-and-microphone-unsafe-delegation"
                ),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareCameraAndMicrophoneUnsafeDelegation2.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("origin"),
                        "%2$S": VARIABLE_REFERENCE("thirdParty"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "webrtc-allow-share-camera-and-audio-capture-unsafe-delegation"
                ),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareCameraAndAudioCaptureUnsafeDelegation2.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("origin"),
                        "%2$S": VARIABLE_REFERENCE("thirdParty"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "webrtc-allow-share-screen-and-microphone-unsafe-delegation"
                ),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareScreenAndMicrophoneUnsafeDelegation2.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("origin"),
                        "%2$S": VARIABLE_REFERENCE("thirdParty"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "webrtc-allow-share-screen-and-audio-capture-unsafe-delegation"
                ),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareScreenAndAudioCaptureUnsafeDelegation2.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("origin"),
                        "%2$S": VARIABLE_REFERENCE("thirdParty"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-speaker-unsafe-delegation"),
                value=REPLACE(
                    browser,
                    "selectAudioOutput.shareSpeakerUnsafeDelegation.message",
                    {
                        "%1$S": VARIABLE_REFERENCE("origin"),
                        "%2$S": VARIABLE_REFERENCE("thirdParty"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-share-screen-warning"),
                value=COPY(browser, "getUserMedia.shareScreenWarning2.message"),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-share-browser-warning"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareFirefoxWarning2.message",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-share-screen-learn-more"),
                value=COPY(browser, "getUserMedia.shareScreen.learnMoreLabel"),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-pick-window-or-screen"),
                value=COPY(browser, "getUserMedia.pickWindowOrScreen.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-share-entire-screen"),
                value=COPY(browser, "getUserMedia.shareEntireScreen.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-share-pipe-wire-portal"),
                value=COPY(browser, "getUserMedia.sharePipeWirePortal.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-share-monitor"),
                value=REPLACE(
                    browser,
                    "getUserMedia.shareMonitor.label",
                    {"%1$S": VARIABLE_REFERENCE("monitorIndex")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-share-application"),
                value=PLURALS(
                    browser,
                    "getUserMedia.shareApplicationWindowCount.label",
                    VARIABLE_REFERENCE("windowCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": VARIABLE_REFERENCE("appName"),
                            "#2": VARIABLE_REFERENCE("windowCount"),
                        },
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-action-allow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser, "getUserMedia.allow.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser, "getUserMedia.allow.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-action-block"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY_PATTERN(
                            browser_ftl, "popup-screen-sharing-block.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            browser_ftl, "popup-screen-sharing-block.accesskey"
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-action-always-block"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY_PATTERN(
                            browser_ftl, "popup-screen-sharing-always-block.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            browser_ftl,
                            "popup-screen-sharing-always-block.accesskey",
                        ),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-action-not-now"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser, "getUserMedia.notNow.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(browser, "getUserMedia.notNow.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-remember-allow-checkbox"),
                value=COPY(browser, "getUserMedia.remember"),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-mute-notifications-checkbox"),
                value=COPY_PATTERN(browser_ftl, "popup-mute-notifications-checkbox"),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-reason-for-no-permanent-allow-screen"),
                value=REPLACE(
                    browser,
                    "getUserMedia.reasonForNoPermanentAllow.screen3",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-reason-for-no-permanent-allow-audio"),
                value=REPLACE(
                    browser,
                    "getUserMedia.reasonForNoPermanentAllow.audio",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("webrtc-reason-for-no-permanent-allow-insecure"),
                value=REPLACE(
                    browser,
                    "getUserMedia.reasonForNoPermanentAllow.insecure",
                    {"%1$S": TERM_REFERENCE("brand-short-name")},
                ),
            ),
        ],
    )

    ctx.add_transforms(
        browser_ftl,
        browser_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("popup-select-window-or-screen"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(browser, "getUserMedia.selectWindowOrScreen2.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            browser, "getUserMedia.selectWindowOrScreen2.accesskey"
                        ),
                    ),
                ],
            ),
        ],
    )
