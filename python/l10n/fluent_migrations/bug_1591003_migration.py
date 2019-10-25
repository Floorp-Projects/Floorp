# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY

def migrate(ctx):
    """Bug 1591003 - Migrate urlbar notification tooltips to Fluent, part {index}"""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from(
            """

urlbar-geolocation-blocked =
    .tooltiptext = { COPY(path1, "urlbar.geolocationBlocked.tooltip") }
urlbar-web-notifications-blocked =
    .tooltiptext = { COPY(path1, "urlbar.webNotificationsBlocked.tooltip") }
urlbar-camera-blocked =
    .tooltiptext = { COPY(path1, "urlbar.cameraBlocked.tooltip") }
urlbar-microphone-blocked =
    .tooltiptext = { COPY(path1, "urlbar.microphoneBlocked.tooltip") }
urlbar-screen-blocked =
    .tooltiptext = { COPY(path1, "urlbar.screenBlocked.tooltip") }
urlbar-persistent-storage-blocked =
    .tooltiptext = { COPY(path1, "urlbar.persistentStorageBlocked.tooltip") }
urlbar-popup-blocked =
    .tooltiptext = { COPY(path1, "urlbar.popupBlocked.tooltip") }
urlbar-autoplay-media-blocked =
    .tooltiptext = { COPY(path1, "urlbar.autoplayMediaBlocked.tooltip") }
urlbar-canvas-blocked =
    .tooltiptext = { COPY(path1, "urlbar.canvasBlocked.tooltip") }
urlbar-midi-blocked =
    .tooltiptext = { COPY(path1, "urlbar.midiBlocked.tooltip") }
urlbar-install-blocked =
    .tooltiptext = { COPY(path1, "urlbar.installBlocked.tooltip") }
""", path1="/browser/chrome/browser/browser.dtd"))
