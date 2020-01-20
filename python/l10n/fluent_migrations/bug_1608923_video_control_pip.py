# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1608923 - add accessible labels to PiP controls, part {index}."""

    ctx.add_transforms(
        'toolkit/toolkit/pictureinpicture/pictureinpicture.ftl',
        'toolkit/toolkit/pictureinpicture/pictureinpicture.ftl',
        transforms_from(
"""
pictureinpicture-pause =
  .aria-label = { COPY(from_path, "playButton.pauseLabel") }
pictureinpicture-play =
  .aria-label = { COPY(from_path, "playButton.playLabel") }

pictureinpicture-mute =
  .aria-label = { COPY(from_path, "muteButton.muteLabel") }
pictureinpicture-unmute =
  .aria-label = { COPY(from_path, "muteButton.unmuteLabel") }
""", from_path="toolkit/chrome/global/videocontrols.dtd")
    )


