# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
# from fluent.migrate.helpers import MESSAGE_REFERENCE, EXTERNAL_ARGUMENT, transforms_from
# from fluent.migrate import COPY, CONCAT, REPLACE


def migrate(ctx):
    """Bug 1457948 - Migrate in-content/privacy.js to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/preferences/permissions.ftl',
        'browser/browser/preferences/permissions.ftl',
        transforms_from(
"""
permissions-invalid-uri-title = { COPY("browser/chrome/browser/preferences/preferences.properties", "invalidURITitle") }
permissions-invalid-uri-label = { COPY("browser/chrome/browser/preferences/preferences.properties", "invalidURI") }
permissions-exceptions-tracking-protection-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "trackingprotectionpermissionstitle") }
    .style = { permissions-window.style }
permissions-exceptions-tracking-protection-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "trackingprotectionpermissionstext2") }
permissions-exceptions-cookie-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "cookiepermissionstitle1") }
    .style = { permissions-window.style }
permissions-exceptions-cookie-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "cookiepermissionstext1") }
permissions-exceptions-popup-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "popuppermissionstitle2") }
    .style = { permissions-window.style }
permissions-exceptions-popup-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "popuppermissionstext") }
permissions-exceptions-saved-logins-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "savedLoginsExceptions_title") }
    .style = { permissions-window.style }
permissions-exceptions-saved-logins-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "savedLoginsExceptions_desc3") }
permissions-exceptions-addons-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "addons_permissions_title2") }
    .style = { permissions-window.style }
permissions-exceptions-addons-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "addonspermissionstext") }
permissions-site-notification-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "notificationspermissionstitle2") }
    .style = { permissions-window.style }
permissions-site-notification-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "notificationspermissionstext6") }
permissions-site-notification-disable-label =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "notificationspermissionsdisablelabel") }
permissions-site-notification-disable-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "notificationspermissionsdisabledescription") }
permissions-site-location-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "locationpermissionstitle") }
    .style = { permissions-window.style }
permissions-site-location-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "locationpermissionstext2") }
permissions-site-location-disable-label =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "locationpermissionsdisablelabel") }
permissions-site-location-disable-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "locationpermissionsdisabledescription") }
permissions-site-camera-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "camerapermissionstitle") }
    .style = { permissions-window.style }
permissions-site-camera-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "camerapermissionstext2") }
permissions-site-camera-disable-label =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "camerapermissionsdisablelabel") }
permissions-site-camera-disable-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "camerapermissionsdisabledescription") }
permissions-site-microphone-window =
    .title = { COPY("browser/chrome/browser/preferences/preferences.properties", "microphonepermissionstitle") }
    .style = { permissions-window.style }
permissions-site-microphone-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "microphonepermissionstext2") }
permissions-site-microphone-disable-label =
    .label = { COPY("browser/chrome/browser/preferences/preferences.properties", "microphonepermissionsdisablelabel") }
permissions-site-microphone-disable-desc = { COPY("browser/chrome/browser/preferences/preferences.properties", "microphonepermissionsdisabledescription") }
""")
    )
