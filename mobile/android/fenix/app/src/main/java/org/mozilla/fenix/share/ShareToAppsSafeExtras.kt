/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import org.mozilla.fenix.GleanMetrics.Events

private const val TELEMETRY_OTHER = "other"

// Set of public packages which we allow for recording to telemetry
private val allowedTelemetryPackages = setOf(
    // Email
    "com.google.android.gm",
    "com.microsoft.office.outlook",
    "com.samsung.android.email.provider",
    // Social Media
    "com.facebook.katana",
    "com.instagram.android",
    "com.snapchat.android",
    // Notes
    "com.google.android.keep",
    "com.samsung.android.app.notes",
    "com.microsoft.office.onenote",
    "com.evernote",
    // Messaging
    "com.google.android.apps.messaging",
    "com.facebook.orca",
    "com.chating.messages.chat.fun",
    "org.telegram.messenger",
    "org.thoughtcrime.securesms",
    "com.google.android.apps.dynamite",
    "com.whatsapp",
    "com.tencent.mm",
    "com.Slack",
    "com.discord",
    // Device Actions
    "com.android.bluetooth",
    "com.google.android.gms",
    "org.mozilla.fenix.COPY_LINK_TO_CLIPBOARD",
)

internal fun getShareToAppSafeExtra(appPackage: String): Events.ShareToAppExtra {
    return if (allowedTelemetryPackages.contains(appPackage)) {
        Events.ShareToAppExtra(appPackage)
    } else {
        Events.ShareToAppExtra(TELEMETRY_OTHER)
    }
}
