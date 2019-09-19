/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webnotifications

import android.app.Notification
import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.core.net.toUri
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.Icon.Source
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.IconRequest.Size
import mozilla.components.concept.engine.webnotifications.WebNotification

internal class NativeNotificationBridge(
    private val icons: BrowserIcons
) {

    /**
     * Create a system [Notification] from this [WebNotification].
     */
    suspend fun convertToAndroidNotification(
        notification: WebNotification,
        context: Context,
        channelId: String
    ): Notification {
        val builder = if (SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(context, channelId)
        } else {
            @Suppress("Deprecation")
            Notification.Builder(context)
        }

        with(notification) {
            loadIcon(iconUrl?.toUri(), origin, Size.DEFAULT)?.let { icon ->
                builder.setLargeIcon(icon)
            }

            builder.setContentTitle(title).setContentText(body)

            @Suppress("Deprecation")
            builder.setVibrate(vibrate)

            timestamp?.let {
                builder.setShowWhen(true).setWhen(it)
            }

            if (silent) {
                @Suppress("Deprecation")
                builder.setDefaults(0)
            }
        }

        return builder.build()
    }

    /**
     * Load an icon for a notification.
     */
    private suspend fun loadIcon(url: Uri?, origin: String, size: Size): Bitmap? {
        url ?: return null
        val icon = icons.loadIcon(IconRequest(
            url = origin,
            size = size,
            resources = listOf(IconRequest.Resource(
                url = url.toString(),
                type = IconRequest.Resource.Type.MANIFEST_ICON
            ))
        )).await()

        return if (icon.source == Source.GENERATOR) null else icon.bitmap
    }
}
