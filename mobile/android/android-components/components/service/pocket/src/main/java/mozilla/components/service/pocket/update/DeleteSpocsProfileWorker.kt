/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.service.pocket.GlobalDependencyProvider

/**
 * WorkManager Worker used for deleting the profile used for downloading Pocket sponsored stories.
 */
internal class DeleteSpocsProfileWorker(
    context: Context,
    params: WorkerParameters,
) : CoroutineWorker(context, params) {

    override suspend fun doWork(): Result {
        return withContext(Dispatchers.IO) {
            if (GlobalDependencyProvider.SponsoredStories.useCases?.deleteProfile?.invoke() == true) {
                Result.success()
            } else {
                Result.retry()
            }
        }
    }

    internal companion object {
        const val DELETE_SPOCS_PROFILE_WORK_TAG =
            "mozilla.components.feature.pocket.spocs.profile.delete.work.tag"
    }
}
