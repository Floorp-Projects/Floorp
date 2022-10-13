/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.loader

import android.graphics.drawable.Drawable
import android.widget.ImageView
import androidx.annotation.MainThread
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.thumbnails.R
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.support.images.CancelOnDetach
import java.lang.ref.WeakReference

/**
 * An implementation of [ImageLoader] for loading thumbnails into a [ImageView].
 */
class ThumbnailLoader(private val storage: ThumbnailStorage) : ImageLoader {

    override fun loadIntoView(
        view: ImageView,
        request: ImageLoadRequest,
        placeholder: Drawable?,
        error: Drawable?,
    ) {
        CoroutineScope(Dispatchers.Main).launch {
            loadIntoViewInternal(WeakReference(view), request, placeholder, error)
        }
    }

    @MainThread
    private suspend fun loadIntoViewInternal(
        view: WeakReference<ImageView>,
        request: ImageLoadRequest,
        placeholder: Drawable?,
        error: Drawable?,
    ) {
        // If we previously started loading into the view, cancel the job.
        val existingJob = view.get()?.getTag(R.id.mozac_browser_thumbnails_tag_job) as? Job
        existingJob?.cancel()

        // Create a loading job
        val deferredThumbnail = storage.loadThumbnail(request)

        view.get()?.setTag(R.id.mozac_browser_thumbnails_tag_job, deferredThumbnail)
        val onAttachStateChangeListener = CancelOnDetach(deferredThumbnail).also {
            view.get()?.addOnAttachStateChangeListener(it)
        }

        try {
            val thumbnail = deferredThumbnail.await()
            if (thumbnail != null) {
                view.get()?.setImageBitmap(thumbnail)
            } else {
                view.get()?.setImageDrawable(placeholder)
            }
        } catch (e: CancellationException) {
            view.get()?.setImageDrawable(error)
        } finally {
            view.get()?.removeOnAttachStateChangeListener(onAttachStateChangeListener)
            view.get()?.setTag(R.id.mozac_browser_thumbnails_tag_job, null)
        }
    }
}
