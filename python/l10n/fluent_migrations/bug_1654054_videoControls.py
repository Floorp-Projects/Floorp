# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1654054 - Port videocontrols to Fluent, part {index}."""

    source = "toolkit/chrome/global/videocontrols.dtd"
    target = "toolkit/toolkit/global/videocontrols.ftl"
    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
videocontrols-play-button =
    .aria-label = { COPY(from_path, "playButton.playLabel") }

videocontrols-pause-button =
    .aria-label = { COPY(from_path, "playButton.pauseLabel") }

videocontrols-mute-button =
    .aria-label = { COPY(from_path, "muteButton.muteLabel") }

videocontrols-unmute-button =
    .aria-label = { COPY(from_path, "muteButton.unmuteLabel") }

videocontrols-enterfullscreen-button =
    .aria-label = { COPY(from_path, "fullscreenButton.enterfullscreenlabel") }

videocontrols-exitfullscreen-button =
    .aria-label = { COPY(from_path, "fullscreenButton.exitfullscreenlabel") }

videocontrols-casting-button-label =
    .aria-label = { COPY(from_path, "castingButton.castingLabel") }

videocontrols-closed-caption-off =
    .offlabel = { COPY(from_path, "closedCaption.off") }

videocontrols-picture-in-picture-label = { COPY(from_path, "pictureInPicture.label") }

videocontrols-picture-in-picture-toggle-label = { COPY(from_path, "pictureInPictureToggle.label") }
""",
            from_path=source,
        ),
    )

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("videocontrols-picture-in-picture-explainer"),
                value=REPLACE(
                    source,
                    "pictureInPictureExplainer",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
        ],
    )

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
videocontrols-error-aborted = { COPY(from_path, "error.aborted") }

videocontrols-error-network = { COPY(from_path, "error.network") }

videocontrols-error-decode = { COPY(from_path, "error.decode") }

videocontrols-error-src-not-supported = { COPY(from_path, "error.srcNotSupported") }

videocontrols-error-no-source = { COPY(from_path, "error.noSource2") }

videocontrols-error-generic = { COPY(from_path, "error.generic") }

videocontrols-status-picture-in-picture = { COPY(from_path, "status.pictureInPicture") }
""",
            from_path=source,
        ),
    )

    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("videocontrols-position-and-duration-labels"),
                value=REPLACE(
                    source,
                    "positionAndDuration.nameFormat",
                    {
                        "<span>": FTL.TextElement(
                            '<span data-l10n-name="position-duration-format">'
                        ),
                        "#1": VARIABLE_REFERENCE("position"),
                        "#2": VARIABLE_REFERENCE("duration"),
                    },
                ),
            ),
        ],
    )
