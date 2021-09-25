/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.withContext
import mozilla.appservices.places.InternalPanic
import mozilla.appservices.places.PlacesException
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.storage.Storage
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.NamedThreadFactory
import mozilla.components.support.utils.logElapsedTime
import java.util.concurrent.Executors

/**
 * A base class for concrete implementations of PlacesStorages
 */
abstract class PlacesStorage(
    context: Context,
    val crashReporter: CrashReporting? = null
) : Storage, SyncableStore {
    internal val writeScope by lazy {
        CoroutineScope(
            Executors.newSingleThreadExecutor(
                NamedThreadFactory("PlacesStorageWriteScope")
            ).asCoroutineDispatcher()
        )
    }
    internal val readScope by lazy { CoroutineScope(Dispatchers.IO) }
    private val storageDir by lazy { context.filesDir }

    abstract val logger: Logger

    @VisibleForTesting
    internal open val places: Connection by lazy {
        RustPlacesConnection.init(storageDir)
        RustPlacesConnection
    }

    internal val writer: PlacesWriterConnection by lazy { places.writer() }
    internal val reader: PlacesReaderConnection by lazy { places.reader() }

    override suspend fun warmUp() {
        logElapsedTime(logger, "Warming up places storage") {
            writer
            reader
        }
    }

    /**
     * Internal database maintenance tasks. Ideally this should be called once a day.
     */
    override suspend fun runMaintenance() {
        withContext(writeScope.coroutineContext) {
            places.writer().runMaintenance()
        }
    }

    /**
     * Cleans up background work and database connections
     */
    override fun cleanup() {
        writeScope.coroutineContext.cancelChildren()
        readScope.coroutineContext.cancelChildren()
        places.close()
    }

    /**
     * Runs [block] described by [operation], ignoring and logging non-fatal exceptions.
     *
     * @param operation the name of the operation to run.
     * @param block the operation to run.
     */
    protected inline fun handlePlacesExceptions(operation: String, block: () -> Unit) {
        try {
            block()
        } catch (e: InternalPanic) {
            throw e
        } catch (e: PlacesException) {
            crashReporter?.submitCaughtException(e)
            logger.warn("Ignoring PlacesException while running $operation", e)
        }
    }

    /**
     * Runs [block] described by [operation] to return a result of type [T], ignoring and
     * logging non-fatal exceptions. In case of a non-fatal exception, the provided
     * [default] value is returned.
     *
     * @param operation the name of the operation to run.
     * @param block the operation to run.
     * @param default the default value to return in case of errors.
     */
    inline fun <T> handlePlacesExceptions(
        operation: String,
        default: T,
        block: () -> T
    ): T {
        return try {
            block()
        } catch (e: InternalPanic) {
            throw e
        } catch (e: PlacesException) {
            crashReporter?.submitCaughtException(e)
            logger.warn("Ignoring PlacesException while running $operation", e)
            default
        }
    }

    /**
     * Runs a [syncBlock], re-throwing any panics that may be encountered.
     * @return [SyncStatus.Ok] on success, or [SyncStatus.Error] on non-panic [PlacesException].
     */
    protected inline fun syncAndHandleExceptions(syncBlock: () -> Unit): SyncStatus {
        return try {
            logger.debug("Syncing...")
            syncBlock()
            logger.debug("Successfully synced.")
            SyncStatus.Ok

            // Order of these catches matters: InternalPanic extends PlacesException.
        } catch (e: InternalPanic) {
            logger.error("Places panic while syncing", e)
            throw e
        } catch (e: PlacesException) {
            logger.error("Places exception while syncing", e)
            SyncStatus.Error(e)
        }
    }
}
