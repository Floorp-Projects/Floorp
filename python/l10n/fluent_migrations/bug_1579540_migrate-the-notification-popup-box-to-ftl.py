# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1579540 - Migrate the notification-popup-box element to ftl, part {index}"""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
            """
urlbar-services-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.servicesNotificationAnchor.tooltip") }
urlbar-web-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.webNotificationAnchor.tooltip") }
urlbar-midi-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.midiNotificationAnchor.tooltip") }
urlbar-eme-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.emeNotificationAnchor.tooltip") }
urlbar-web-authn-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.webAuthnAnchor.tooltip") }
urlbar-canvas-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.canvasNotificationAnchor.tooltip") }
urlbar-web-rtc-share-microphone-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.webRTCShareMicrophoneNotificationAnchor.tooltip") }
urlbar-default-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.defaultNotificationAnchor.tooltip") }
urlbar-geolocation-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.geolocationNotificationAnchor.tooltip") }
urlbar-storage-access-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.storageAccessAnchor.tooltip") }
urlbar-translate-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.translateNotificationAnchor.tooltip") }
urlbar-web-rtc-share-screen-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.webRTCShareScreenNotificationAnchor.tooltip") }
urlbar-indexed-db-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.indexedDBNotificationAnchor.tooltip") }
urlbar-password-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.passwordNotificationAnchor.tooltip") }
urlbar-translated-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.translatedNotificationAnchor.tooltip") }
urlbar-plugins-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.pluginsNotificationAnchor.tooltip") }
urlbar-web-rtc-share-devices-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.webRTCShareDevicesNotificationAnchor.tooltip") }
urlbar-autoplay-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.autoplayNotificationAnchor.tooltip") }
urlbar-persistent-storage-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.persistentStorageNotificationAnchor.tooltip") }
urlbar-addons-notification-anchor =
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "urlbar.addonsNotificationAnchor.tooltip") }
"""
        )
    )

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
            """
urlbar-identity-button =
    .aria-label = {COPY_PATTERN(from_path, "browser-urlbar-identity-button.aria-label")}
""",
            from_path='browser/browser/browser.ftl'),
    )
