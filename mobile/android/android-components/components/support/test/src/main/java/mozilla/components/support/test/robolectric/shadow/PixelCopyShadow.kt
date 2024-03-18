/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.robolectric.shadow

import android.annotation.TargetApi
import android.graphics.Bitmap
import android.graphics.Rect
import android.os.Build
import android.os.Handler
import android.view.PixelCopy
import android.view.Window
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.Implements

/**
 * Shadow for [PixelCopy] API.
 */
@Implements(PixelCopy::class, minSdk = Build.VERSION_CODES.N)
@TargetApi(Build.VERSION_CODES.N)
class PixelCopyShadow {

    companion object {
        var copyResult = PixelCopy.SUCCESS

        @JvmStatic
        @Implementation
        // Some parameters are unused but method signature should be the same as for original class.
        @Suppress("UNUSED_PARAMETER")
        fun request(
            source: Window,
            srcRect: Rect?,
            dest: Bitmap,
            listener: PixelCopy.OnPixelCopyFinishedListener,
            listenerThread: Handler?,
        ) {
            listener.onPixelCopyFinished(copyResult)
        }
    }
}
