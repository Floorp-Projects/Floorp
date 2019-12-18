# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, MESSAGE_REFERENCE
from fluent.migrate import COPY_PATTERN, REPLACE, COPY


def migrate(ctx):
    """Bug 1600528 - Migrate Migrate browserContext.inc to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/browserContext.ftl',
        'browser/browser/browserContext.ftl',
        transforms_from(
"""
navbar-tooltip-instruction =
    .value = { PLATFORM() ->
        [macos] { COPY(from_path, "backForwardButtonMenuMac.tooltip") }
       *[other] { COPY(from_path, "backForwardButtonMenu.tooltip") }
    }

main-context-menu-back =
    .tooltiptext = { COPY(from_path, "backButton.tooltip") }
    .aria-label = { COPY(from_path, "backCmd.label") }
    .accesskey = { COPY(from_path, "backCmd.accesskey") }

navbar-tooltip-back =
    .value = { main-context-menu-back.tooltiptext }

toolbar-button-back =
    .label = { main-context-menu-back.aria-label }

main-context-menu-forward =
    .tooltiptext = { COPY(from_path, "forwardButton.tooltip") }
    .aria-label = { COPY(from_path, "forwardCmd.label") }
    .accesskey = { COPY(from_path, "forwardCmd.accesskey") }

navbar-tooltip-forward =
    .value = { main-context-menu-forward.tooltiptext }

toolbar-button-forward =
    .label = { main-context-menu-forward.aria-label }

main-context-menu-reload =
    .aria-label = { COPY(from_path, "reloadCmd.label") }
    .accesskey = { COPY(from_path, "reloadCmd.accesskey") }

toolbar-button-reload =
    .label = { main-context-menu-reload.aria-label }

main-context-menu-stop =
    .aria-label = { COPY(from_path, "stopCmd.label") }
    .accesskey = { COPY(from_path, "stopCmd.accesskey") }

toolbar-button-stop =
    .label = { main-context-menu-stop.aria-label }

toolbar-button-stop-reload =
    .title = { main-context-menu-reload.aria-label }

main-context-menu-page-save =
    .label = { COPY(from_path, "savePageCmd.label") }
    .accesskey = { COPY(from_path, "savePageCmd.accesskey2") }

toolbar-button-page-save =
    .label = { main-context-menu-page-save.label }

main-context-menu-bookmark-page =
    .aria-label = { COPY(from_path, "bookmarkPageCmd2.label") }
    .accesskey = { COPY(from_path, "bookmarkPageCmd2.accesskey") }

main-context-menu-open-link =
    .label = { COPY(from_path, "openLinkCmdInCurrent.label") }
    .accesskey = { COPY(from_path, "openLinkCmdInCurrent.accesskey") }

main-context-menu-open-link-new-tab =
    .label = { COPY(from_path, "openLinkCmdInTab.label") }
    .accesskey = { COPY(from_path, "openLinkCmdInTab.accesskey") }

main-context-menu-open-link-container-tab =
    .label = { COPY(from_path, "openLinkCmdInContainerTab.label") }
    .accesskey = { COPY(from_path, "openLinkCmdInContainerTab.accesskey") }

main-context-menu-open-link-new-window =
    .label = { COPY(from_path, "openLinkCmd.label") }
    .accesskey = { COPY(from_path, "openLinkCmd.accesskey") }

main-context-menu-open-link-new-private-window =
    .label = { COPY(from_path, "openLinkInPrivateWindowCmd.label") }
    .accesskey = { COPY(from_path, "openLinkInPrivateWindowCmd.accesskey") }

main-context-menu-bookmark-this-link =
    .label = { COPY(from_path, "bookmarkThisLinkCmd.label") }
    .accesskey = { COPY(from_path, "bookmarkThisLinkCmd.accesskey") }

main-context-menu-save-link =
    .label = { COPY(from_path, "saveLinkCmd.label") }
    .accesskey = { COPY(from_path, "saveLinkCmd.accesskey") }

main-context-menu-copy-email =
    .label = { COPY(from_path, "copyEmailCmd.label") }
    .accesskey = { COPY(from_path, "copyEmailCmd.accesskey") }

main-context-menu-copy-link =
    .label = { COPY(from_path, "copyLinkCmd.label") }
    .accesskey = { COPY(from_path, "copyLinkCmd.accesskey") }

main-context-menu-media-play =
    .label = { COPY(from_path, "mediaPlay.label") }
    .accesskey = { COPY(from_path, "mediaPlay.accesskey") }

main-context-menu-media-pause =
    .label = { COPY(from_path, "mediaPause.label") }
    .accesskey = { COPY(from_path, "mediaPause.accesskey") }

main-context-menu-media-mute =
    .label = { COPY(from_path, "mediaMute.label") }
    .accesskey = { COPY(from_path, "mediaMute.accesskey") }

main-context-menu-media-unmute =
    .label = { COPY(from_path, "mediaUnmute.label") }
    .accesskey = { COPY(from_path, "mediaUnmute.accesskey") }

main-context-menu-media-play-speed =
    .label = { COPY(from_path, "mediaPlaybackRate2.label") }
    .accesskey = { COPY(from_path, "mediaPlaybackRate2.accesskey") }

main-context-menu-media-play-speed-slow =
    .label = { COPY(from_path, "mediaPlaybackRate050x2.label") }
    .accesskey = { COPY(from_path, "mediaPlaybackRate050x2.accesskey") }

main-context-menu-media-play-speed-normal =
    .label = { COPY(from_path, "mediaPlaybackRate100x2.label") }
    .accesskey = { COPY(from_path, "mediaPlaybackRate100x2.accesskey") }

main-context-menu-media-play-speed-fast =
    .label = { COPY(from_path, "mediaPlaybackRate125x2.label") }
    .accesskey = { COPY(from_path, "mediaPlaybackRate125x2.accesskey") }

main-context-menu-media-play-speed-faster =
    .label = { COPY(from_path, "mediaPlaybackRate150x2.label") }
    .accesskey = { COPY(from_path, "mediaPlaybackRate150x2.accesskey") }

main-context-menu-media-play-speed-fastest =
    .label = { COPY(from_path, "mediaPlaybackRate200x2.label") }
    .accesskey = { COPY(from_path, "mediaPlaybackRate200x2.accesskey") }

main-context-menu-media-loop =
    .label = { COPY(from_path, "mediaLoop.label") }
    .accesskey = { COPY(from_path, "mediaLoop.accesskey") }

main-context-menu-media-show-controls =
    .label = { COPY(from_path, "mediaShowControls.label") }
    .accesskey = { COPY(from_path, "mediaShowControls.accesskey") }

main-context-menu-media-hide-controls =
    .label = { COPY(from_path, "mediaHideControls.label") }
    .accesskey = { COPY(from_path, "mediaHideControls.accesskey") }

main-context-menu-media-video-fullscreen =
    .label = { COPY(from_path, "videoFullScreen.label") }
    .accesskey = { COPY(from_path, "videoFullScreen.accesskey") }

main-context-menu-media-video-leave-fullscreen =
    .label = { COPY(from_path, "leaveDOMFullScreen.label") }
    .accesskey = { COPY(from_path, "leaveDOMFullScreen.accesskey") }

main-context-menu-media-pip =
    .label = { COPY(from_path, "pictureInPicture.label") }
    .accesskey = { COPY(from_path, "pictureInPicture.accesskey") }

main-context-menu-image-reload =
    .label = { COPY(from_path, "reloadImageCmd.label") }
    .accesskey = { COPY(from_path, "reloadImageCmd.accesskey") }

main-context-menu-image-view =
    .label = { COPY(from_path, "viewImageCmd.label") }
    .accesskey = { COPY(from_path, "viewImageCmd.accesskey") }

main-context-menu-video-view =
    .label = { COPY(from_path, "viewVideoCmd.label") }
    .accesskey = { COPY(from_path, "viewVideoCmd.accesskey") }

main-context-menu-image-copy =
    .label = { COPY(from_path, "copyImageContentsCmd.label") }
    .accesskey = { COPY(from_path, "copyImageContentsCmd.accesskey") }

main-context-menu-image-copy-location =
    .label = { COPY(from_path, "copyImageCmd.label") }
    .accesskey = { COPY(from_path, "copyImageCmd.accesskey") }

main-context-menu-video-copy-location =
    .label = { COPY(from_path, "copyVideoURLCmd.label") }
    .accesskey = { COPY(from_path, "copyVideoURLCmd.accesskey") }

main-context-menu-audio-copy-location =
    .label = { COPY(from_path, "copyAudioURLCmd.label") }
    .accesskey = { COPY(from_path, "copyAudioURLCmd.accesskey") }

main-context-menu-image-save-as =
    .label = { COPY(from_path, "saveImageCmd.label") }
    .accesskey = { COPY(from_path, "saveImageCmd.accesskey") }

main-context-menu-image-email =
    .label = { COPY(from_path, "emailImageCmd.label") }
    .accesskey = { COPY(from_path, "emailImageCmd.accesskey") }

main-context-menu-image-set-as-background =
    .label = { COPY(from_path, "setDesktopBackgroundCmd.label") }
    .accesskey = { COPY(from_path, "setDesktopBackgroundCmd.accesskey") }

main-context-menu-image-info =
    .label = { COPY(from_path, "viewImageInfoCmd.label") }
    .accesskey = { COPY(from_path, "viewImageInfoCmd.accesskey") }

main-context-menu-image-desc =
    .label = { COPY(from_path, "viewImageDescCmd.label") }
    .accesskey = { COPY(from_path, "viewImageDescCmd.accesskey") }

main-context-menu-video-save-as =
    .label = { COPY(from_path, "saveVideoCmd.label") }
    .accesskey = { COPY(from_path, "saveVideoCmd.accesskey") }

main-context-menu-audio-save-as =
    .label = { COPY(from_path, "saveAudioCmd.label") }
    .accesskey = { COPY(from_path, "saveAudioCmd.accesskey") }

main-context-menu-video-image-save-as =
    .label = { COPY(from_path, "videoSaveImage.label") }
    .accesskey = { COPY(from_path, "videoSaveImage.accesskey") }

main-context-menu-video-email =
    .label = { COPY(from_path, "emailVideoCmd.label") }
    .accesskey = { COPY(from_path, "emailVideoCmd.accesskey") }

main-context-menu-audio-email =
    .label = { COPY(from_path, "emailAudioCmd.label") }
    .accesskey = { COPY(from_path, "emailAudioCmd.accesskey") }

main-context-menu-plugin-play =
    .label = { COPY(from_path, "playPluginCmd.label") }
    .accesskey = { COPY(from_path, "playPluginCmd.accesskey") }

main-context-menu-plugin-hide =
    .label = { COPY(from_path, "hidePluginCmd.label") }
    .accesskey = { COPY(from_path, "hidePluginCmd.accesskey") }

main-context-menu-send-to-device =
    .label = { COPY(from_path, "sendPageToDevice.label") }
    .accesskey = { COPY(from_path, "sendPageToDevice.accesskey") }

main-context-menu-view-background-image =
    .label = { COPY(from_path, "viewBGImageCmd.label") }
    .accesskey = { COPY(from_path, "viewBGImageCmd.accesskey") }

main-context-menu-keyword =
    .label = { COPY(from_path, "keywordfield.label") }
    .accesskey = { COPY(from_path, "keywordfield.accesskey") }

main-context-menu-link-send-to-device =
    .label = { COPY(from_path, "sendLinkToDevice.label") }
    .accesskey = { COPY(from_path, "sendLinkToDevice.accesskey") }

main-context-menu-frame =
    .label = { COPY(from_path, "thisFrameMenu.label") }
    .accesskey = { COPY(from_path, "thisFrameMenu.accesskey") }

main-context-menu-frame-show-this =
    .label = { COPY(from_path, "showOnlyThisFrameCmd.label") }
    .accesskey = { COPY(from_path, "showOnlyThisFrameCmd.accesskey") }

main-context-menu-frame-open-tab =
    .label = { COPY(from_path, "openFrameCmdInTab.label") }
    .accesskey = { COPY(from_path, "openFrameCmdInTab.accesskey") }

main-context-menu-frame-open-window =
    .label = { COPY(from_path, "openFrameCmd.label") }
    .accesskey = { COPY(from_path, "openFrameCmd.accesskey") }

main-context-menu-frame-reload =
    .label = { COPY(from_path, "reloadFrameCmd.label") }
    .accesskey = { COPY(from_path, "reloadFrameCmd.accesskey") }

main-context-menu-frame-bookmark =
    .label = { COPY(from_path, "bookmarkThisFrameCmd.label") }
    .accesskey = { COPY(from_path, "bookmarkThisFrameCmd.accesskey") }

main-context-menu-frame-save-as =
    .label = { COPY(from_path, "saveFrameCmd.label") }
    .accesskey = { COPY(from_path, "saveFrameCmd.accesskey") }

main-context-menu-frame-print =
    .label = { COPY(from_path, "printFrameCmd.label") }
    .accesskey = { COPY(from_path, "printFrameCmd.accesskey") }

main-context-menu-frame-view-source =
    .label = { COPY(from_path, "viewFrameSourceCmd.label") }
    .accesskey = { COPY(from_path, "viewFrameSourceCmd.accesskey") }

main-context-menu-frame-view-info =
    .label = { COPY(from_path, "viewFrameInfoCmd.label") }
    .accesskey = { COPY(from_path, "viewFrameInfoCmd.accesskey") }

main-context-menu-view-selection-source =
    .label = { COPY(from_path, "viewPartialSourceForSelectionCmd.label") }
    .accesskey = { COPY(from_path, "viewPartialSourceCmd.accesskey") }

main-context-menu-view-page-source =
    .label = { COPY(from_path, "viewPageSourceCmd.label") }
    .accesskey = { COPY(from_path, "viewPageSourceCmd.accesskey") }

main-context-menu-view-page-info =
    .label = { COPY(from_path, "viewPageInfoCmd.label") }
    .accesskey = { COPY(from_path, "viewPageInfoCmd.accesskey") }

main-context-menu-bidi-switch-text =
    .label = { COPY(from_path, "bidiSwitchTextDirectionItem.label") }
    .accesskey = { COPY(from_path, "bidiSwitchTextDirectionItem.accesskey") }

main-context-menu-bidi-switch-page =
    .label = { COPY(from_path, "bidiSwitchPageDirectionItem.label") }
    .accesskey = { COPY(from_path, "bidiSwitchPageDirectionItem.accesskey") }

main-context-menu-inspect-element =
    .label = { COPY(from_path, "inspectContextMenu.label") }
    .accesskey = { COPY(from_path, "inspectContextMenu.accesskey") }

main-context-menu-inspect-a11y-properties =
    .label = { COPY(from_path, "inspectA11YContextMenu.label") }

main-context-menu-eme-learn-more =
    .label = { COPY(from_path, "emeLearnMoreContextMenu.label") }
    .accesskey = { COPY(from_path, "emeLearnMoreContextMenu.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd")
    )

    ctx.add_transforms(
        'browser/browser/browserContext.ftl',
        'browser/browser/browserContext.ftl',
        [
            FTL.Message(
                id=FTL.Identifier("main-context-menu-save-link-to-pocket"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "saveLinkToPocketCmd.label",
                            {
                                "Pocket": TERM_REFERENCE("pocket-brand-name")
                            },
                            normalize_printf=True
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "saveLinkToPocketCmd.accesskey"
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("main-context-menu-save-to-pocket"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "saveToPocketCmd.label",
                            {
                                "Pocket": TERM_REFERENCE("pocket-brand-name")
                            },
                            normalize_printf=True
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "saveToPocketCmd.accesskey"
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("main-context-menu-save-to-pocket"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.dtd",
                            "saveToPocketCmd.label",
                            {
                                "Pocket": TERM_REFERENCE("pocket-brand-name")
                            },
                            normalize_printf=True
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            "browser/chrome/browser/browser.dtd",
                            "saveToPocketCmd.accesskey"
                        )
                    ),
                ]
            ),
        ]
    )

    ctx.add_transforms(
        'toolkit/toolkit/global/textActions.ftl',
        'toolkit/toolkit/global/textActions.ftl',
        transforms_from(
"""
text-action-undo =
    .label = { COPY(from_path, "undoCmd.label") }
    .accesskey = { COPY(from_path, "undoCmd.accesskey") }

text-action-cut =
    .label = { COPY(from_path, "cutCmd.label") }
    .accesskey = { COPY(from_path, "cutCmd.accesskey") }

text-action-copy =
    .label = { COPY(from_path, "copyCmd.label") }
    .accesskey = { COPY(from_path, "copyCmd.accesskey") }

text-action-paste =
    .label = { COPY(from_path, "pasteCmd.label") }
    .accesskey = { COPY(from_path, "pasteCmd.accesskey") }

text-action-delete =
    .label = { COPY(from_path, "deleteCmd.label") }
    .accesskey = { COPY(from_path, "deleteCmd.accesskey") }

text-action-select-all =
    .label = { COPY(from_path, "selectAllCmd.label") }
    .accesskey = { COPY(from_path, "selectAllCmd.accesskey") }
""", from_path="browser/chrome/browser/browser.dtd")
    )
