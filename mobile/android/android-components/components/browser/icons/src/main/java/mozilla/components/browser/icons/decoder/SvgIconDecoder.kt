/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.decoder

import android.content.Context
import android.graphics.Bitmap
import androidx.core.graphics.drawable.toBitmap
import coil.ImageLoader
import coil.decode.SvgDecoder
import coil.executeBlocking
import coil.request.ImageRequest
import mozilla.components.support.images.DesiredSize
import mozilla.components.support.images.decoder.ImageDecoder

/**
 * [ImageDecoder] that will use the Coil library [SvgDecoder.Factory] in order to decode the byte data.
 *
 * ⚠️ For guidance on use of the Coil library see comment for [ComponentsDependencies.thirdparty_coil_svg].
 */
class SvgIconDecoder(val context: Context) : ImageDecoder {

    override fun decode(data: ByteArray, desiredSize: DesiredSize): Bitmap? {
        val request = ImageRequest.Builder(context)
            .data(data)
            .build()

        return SvgImageLoader.getInstance(context).executeBlocking(request).drawable?.toBitmap()
    }

    private object SvgImageLoader {
        @Volatile
        private var instance: ImageLoader? = null

        /**
         *  Gets the [instance]. If [instance] is null also initialise the [ImageLoader].
         *  @return the instance of the [ImageLoader].
         */
        fun getInstance(context: Context): ImageLoader {
            instance?.let { return it }

            synchronized(this) {
                return ImageLoader.Builder(context)
                    .components { add(SvgDecoder.Factory()) }
                    .build().also { instance = it }
            }
        }
    }
}
