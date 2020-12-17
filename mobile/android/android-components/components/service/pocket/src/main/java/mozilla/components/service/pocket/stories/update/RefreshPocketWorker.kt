/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.update

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.CoroutineWorker
import androidx.work.Data
import androidx.work.WorkerParameters
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.service.pocket.DEFAULT_STORIES_COUNT
import mozilla.components.service.pocket.R
import mozilla.components.service.pocket.stories.PocketStoriesUseCases

/**
 * WorkManager Worker used for downloading and persisting locally a new list of Pocket recommended stories.
 */
internal class RefreshPocketWorker(
    private val context: Context,
    params: WorkerParameters
) : CoroutineWorker(context, params) {

    override suspend fun doWork(): Result {

        val pocketItemsLocale = inputData.getString(KEY_EXTRA_DATA_POCKET_ITEMS_LOCALE)
        val pocketItemsCount = inputData.getInt(KEY_EXTRA_DATA_POCKET_ITEMS_COUNT,
            DEFAULT_STORIES_COUNT
        )

        return withContext(Dispatchers.IO) {
            if (PocketStoriesUseCases().RefreshPocketStories(
                    context, pocketItemsCount, pocketItemsLocale!!
                ).invoke()
            ) {
                Result.success()
            } else {
                Result.retry()
            }
        }
    }

    internal companion object {
        const val REFRESH_WORK_TAG =
            "mozilla.components.feature.pocket.recommendations.refresh.work.tag"
        @VisibleForTesting
        val KEY_EXTRA_DATA_POCKET_ITEMS_COUNT = R.id.payload_pocket_items_count.toString()
        @VisibleForTesting
        val KEY_EXTRA_DATA_POCKET_ITEMS_LOCALE = R.id.payload_pocket_items_locale.toString()

        /**
         * Convenience method for configuring various aspects of how/what Pocket stories to download.
         *
         * Use the result in a [androidx.work.WorkRequest.Builder] chain.
         */
        internal fun getPopulatedWorkerData(
            storiesCount: Int,
            locale: String?
        ): Data {
            return Data.Builder()
                .putInt(KEY_EXTRA_DATA_POCKET_ITEMS_COUNT, storiesCount)
                .putString(KEY_EXTRA_DATA_POCKET_ITEMS_LOCALE, locale)
                .build()
        }
    }
}
