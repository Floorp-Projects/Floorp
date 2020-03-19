/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.ext

import android.content.Context
import android.graphics.Bitmap
import mozilla.components.browser.state.state.SessionState
import mozilla.components.feature.media.R

internal fun SessionState?.getTitleOrUrl(context: Context): String = when {
    this == null -> context.getString(R.string.mozac_feature_media_notification_private_mode)
    content.private -> context.getString(R.string.mozac_feature_media_notification_private_mode)
    content.title.isNotEmpty() -> content.title
    else -> content.url
}

internal val SessionState?.nonPrivateUrl
    get() = if (this == null || content.private) "" else content.url

internal val SessionState?.nonPrivateIcon: Bitmap?
    get() = if (this == null || content.private) null else content.icon
