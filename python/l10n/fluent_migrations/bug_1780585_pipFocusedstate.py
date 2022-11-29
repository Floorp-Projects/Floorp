# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1780585 - Enhanced focused states on PiP controls, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/pictureinpicture/pictureinpicture.ftl",
        "toolkit/toolkit/pictureinpicture/pictureinpicture.ftl",
        transforms_from(
            """
pictureinpicture-pause-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-pause-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-pause-cmd.title")}
pictureinpicture-play-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-play-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-play-cmd.title")}
pictureinpicture-mute-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-mute-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-mute-cmd.title")}
pictureinpicture-unmute-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-unmute-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-unmute-cmd.title")}
pictureinpicture-unpip-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-unpip-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-unpip-cmd.title")}
pictureinpicture-close-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-close-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-close-cmd.title")}
pictureinpicture-subtitles-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-subtitles-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-subtitles-cmd.title")}
pictureinpicture-fullscreen-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-fullscreen-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-fullscreen-cmd.title")}
pictureinpicture-exit-fullscreen-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-exit-fullscreen-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-exit-fullscreen-cmd.title")}
pictureinpicture-seekbackward-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-seekbackward-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-seekbackward-cmd.title")}
pictureinpicture-seekforward-btn =
    .aria-label = {COPY_PATTERN(from_path, "pictureinpicture-seekforward-cmd.aria-label")}
    .tooltip = {COPY_PATTERN(from_path, "pictureinpicture-seekforward-cmd.title")}
    """,
            from_path="toolkit/toolkit/pictureinpicture/pictureinpicture.ftl",
        ),
    )
