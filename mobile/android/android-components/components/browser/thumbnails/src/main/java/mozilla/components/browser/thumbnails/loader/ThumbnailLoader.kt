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
import mozilla.components.support.images.CancelOnDetach
import mozilla.components.support.images.loader.ImageLoader
import java.lang.ref.WeakReference

/**
 * An implementation of [ImageLoader] for loading thumbnails into a [ImageView].
 */
class ThumbnailLoader(private val storage: ThumbnailStorage) : ImageLoader {

    override fun loadIntoView(
        view: ImageView,
        id: String,
        placeholder: Drawable?,
        error: Drawable?
    ) {
        CoroutineScope(Dispatchers.Main).launch {
            loadIntoViewInternal(WeakReference(view), id, placeholder, error)
        }
    }

    @MainThread
    private suspend fun loadIntoViewInternal(
        view: WeakReference<ImageView>,
        id: String,
        placeholder: Drawable?,
        error: Drawable?
    ) {
        // If we previously started loading into the view, cancel the job.
        val existingJob = view.get()?.getTag(R.id.mozac_browser_thumbnails_tag_job) as? Job
        existingJob?.cancel()

        view.get()?.setImageDrawable(placeholder)

        // Create a loading job
        val deferredThumbnail = storage.loadThumbnail(id)

        view.get()?.setTag(R.id.mozac_browser_thumbnails_tag_job, deferredThumbnail)
        val onAttachStateChangeListener = CancelOnDetach(deferredThumbnail).also {
            view.get()?.addOnAttachStateChangeListener(it)
        }

        try {
            val thumbnail = deferredThumbnail.await()
            view.get()?.setImageBitmap(thumbnail)
        } catch (e: CancellationException) {
            view.get()?.setImageDrawable(error)
        } finally {
            view.get()?.removeOnAttachStateChangeListener(onAttachStateChangeListener)
            view.get()?.setTag(R.id.mozac_browser_thumbnails_tag_job, null)
        }
    }
}
