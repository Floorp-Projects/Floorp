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
import mozilla.appservices.places.PlacesReaderConnection
import mozilla.appservices.places.PlacesWriterConnection
import mozilla.appservices.places.uniffi.PlacesException
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.storage.Storage
import mozilla.components.concept.storage.StorageMaintenanceRegistry
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
    val crashReporter: CrashReporting? = null,
) : Storage, SyncableStore, StorageMaintenanceRegistry {
    internal var writeScope =
        CoroutineScope(
            Executors.newSingleThreadExecutor(
                NamedThreadFactory("PlacesStorageWriteScope"),
            ).asCoroutineDispatcher(),
        )
        @VisibleForTesting internal set

    internal var readScope = CoroutineScope(Dispatchers.IO)
        @VisibleForTesting internal set
    private val storageDir by lazy { context.filesDir }

    abstract val logger: Logger

    internal open val places: Connection by lazy {
        RustPlacesConnection.init(storageDir)
        RustPlacesConnection
    }

    internal open val writer: PlacesWriterConnection by lazy { places.writer() }
    internal open val reader: PlacesReaderConnection by lazy { places.reader() }

    override suspend fun warmUp() {
        logElapsedTime(logger, "Warming up places storage") {
            writer
            reader
        }
    }

    /**
     * Internal database maintenance tasks. Ideally this should be called once a day.
     *
     * @param dbSizeLimit Maximum DB size to aim for, in bytes. If the
     * database exceeds this size, a small number of visits will be pruned.
     */
    override suspend fun runMaintenance(dbSizeLimit: UInt) {
        withContext(writeScope.coroutineContext) {
            places.writer().runMaintenance(dbSizeLimit)
        }
    }

    @Deprecated(
        "Use `cancelWrites` and `cancelReads` to get a similar functionality. " +
            "See https://github.com/mozilla-mobile/android-components/issues/7348 for a description of the issues " +
            "for when using this method",
    )
    override fun cleanup() {
        writeScope.coroutineContext.cancelChildren()
        readScope.coroutineContext.cancelChildren()
        places.close()
    }

    override fun cancelWrites() {
        interruptCurrentWrites()
        writeScope.coroutineContext.cancelChildren()
    }

    override fun cancelReads() {
        interruptCurrentReads()
        readScope.coroutineContext.cancelChildren()
    }

    /**
     * Stop all current write operations.
     * Allows immediately dismissing all write operations and clearing the write queue.
     */
    internal fun interruptCurrentWrites() {
        handlePlacesExceptions("interruptCurrentWrites") {
            writer.interrupt()
        }
    }

    /**
     * Stop all current read queries.
     * Allows avoiding having to wait for stale queries responses and clears the queries queue.
     */
    internal fun interruptCurrentReads() {
        handlePlacesExceptions("interruptCurrentReads") {
            reader.interrupt()
        }
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
        } catch (e: PlacesException.UnexpectedPlacesException) {
            throw e
        } catch (e: PlacesException.OperationInterrupted) {
            logger.debug("Ignoring expected OperationInterrupted exception when running $operation", e)
        } catch (e: PlacesException.UrlParseFailed) {
            // it's not uncommon to get a mal-formed url, probably the user typing.
            logger.debug("Ignoring invalid URL while running $operation", e)
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
        block: () -> T,
    ): T {
        return try {
            block()
        } catch (e: PlacesException.UnexpectedPlacesException) {
            throw e
        } catch (e: PlacesException.OperationInterrupted) {
            logger.debug("Ignoring expected OperationInterrupted exception when running $operation", e)
            default
        } catch (e: PlacesException.UrlParseFailed) {
            // it's not uncommon to get a mal-formed url, probably the user typing.
            logger.debug("Ignoring invalid URL while running $operation", e)
            default
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

            // Order of these catches matters: UnexpectedPlacesException extends PlacesException
        } catch (e: PlacesException.UnexpectedPlacesException) {
            logger.error("Places panic while syncing", e)
            throw e
        } catch (e: PlacesException) {
            logger.error("Places exception while syncing", e)
            SyncStatus.Error(e)
        }
    }

    /**
     * Registers a storage maintenance worker that prunes database when its size exceeds a size limit.
     * */
    override fun registerStorageMaintenanceWorker() {
        // See child classes for implementation details, it is not implemented by default
    }

    /**
     * Unregisters the storage maintenance worker that is registered
     * by [PlacesStorage.registerStorageMaintenanceWorker].
     *
     * @param uniqueWorkName Unique name of the work request that needs to be unregistered
     * */
    override fun unregisterStorageMaintenanceWorker(uniqueWorkName: String) {
        // See child classes for implementation details, it is not implemented by default
    }
}
