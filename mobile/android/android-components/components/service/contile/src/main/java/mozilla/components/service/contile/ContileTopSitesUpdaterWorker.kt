/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.support.base.log.logger.Logger

/**
 * An implementation of [CoroutineWorker] to perform Contile top site updates.
 */
internal class ContileTopSitesUpdaterWorker(
    context: Context,
    params: WorkerParameters,
) : CoroutineWorker(context, params) {

    private val logger = Logger("ContileTopSitesUpdaterWorker")

    @Suppress("TooGenericExceptionCaught")
    override suspend fun doWork(): Result = withContext(Dispatchers.IO) {
        try {
            ContileTopSitesUseCases().refreshContileTopSites.invoke()
            Result.success()
        } catch (e: Exception) {
            logger.error("Failed to refresh Contile top sites", e)
            Result.failure()
        }
    }
}
