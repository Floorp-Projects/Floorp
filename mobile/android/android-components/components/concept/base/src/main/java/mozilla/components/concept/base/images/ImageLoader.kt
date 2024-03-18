/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.base.images

import android.graphics.drawable.Drawable
import android.widget.ImageView
import androidx.annotation.MainThread

/**
 * A loader that can load an image from an ID directly into an [ImageView].
 */
interface ImageLoader {

    /**
     * Loads an image asynchronously and then displays it in the [ImageView].
     * If the view is detached from the window before loading is completed, then loading is cancelled.
     *
     * @param view [ImageView] to load the image into.
     * @param request [ImageLoadRequest] Load image for this given request.
     * @param placeholder [Drawable] to display while image is loading.
     * @param error [Drawable] to display if loading fails.
     */
    @MainThread
    fun loadIntoView(
        view: ImageView,
        request: ImageLoadRequest,
        placeholder: Drawable? = null,
        error: Drawable? = null,
    )
}
