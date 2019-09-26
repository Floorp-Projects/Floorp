/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.ext

import android.content.Context
import android.graphics.Bitmap
import mozilla.components.browser.session.Session
import mozilla.components.feature.media.R

internal fun Session?.getTitleOrUrl(context: Context): String = when {
    this == null -> context.getString(R.string.mozac_feature_media_notification_private_mode)
    private -> context.getString(R.string.mozac_feature_media_notification_private_mode)
    title.isNotEmpty() -> title
    else -> url
}

internal val Session.nonPrivateUrl
    get() = if (private) "" else url

internal val Session.nonPrivateIcon: Bitmap?
    get() = if (private) null else icon
