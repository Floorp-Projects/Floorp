/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images.ext

import android.widget.ImageView
import mozilla.components.support.images.ImageLoadRequest
import mozilla.components.support.images.loader.ImageLoader
import kotlin.math.max

/**
 * Loads an image asynchronously and then displays it in the [ImageView].
 * If the view is detached from the window before loading is completed, then loading is cancelled.
 *
 * @param view [ImageView] to load the image into.
 * @param id Load image for this given ID.
 */
fun ImageLoader.loadIntoView(view: ImageView, id: String) =
    loadIntoView(view, ImageLoadRequest(id, max(view.height, view.width)))
