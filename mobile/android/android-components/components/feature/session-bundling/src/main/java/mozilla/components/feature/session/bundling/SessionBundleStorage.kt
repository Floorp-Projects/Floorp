/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling

import android.content.Context
import androidx.annotation.CheckResult
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import androidx.lifecycle.LiveData
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.lifecycle.Transformations
import androidx.paging.DataSource
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.ext.writeSnapshot
import mozilla.components.browser.session.storage.AutoSave
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.bundling.adapter.SessionBundleAdapter
import mozilla.components.feature.session.bundling.db.BundleDatabase
import mozilla.components.feature.session.bundling.db.BundleEntity
import mozilla.components.feature.session.bundling.ext.toBundleEntity
import mozilla.components.support.ktx.java.io.truncateDirectory
import java.io.File
import java.lang.IllegalArgumentException
import java.util.concurrent.TimeUnit

/**
 * A [Session] storage implementation that saves snapshots as a [SessionBundle].
 *
 * @param bundleLifetime The lifetime of a bundle controls whether a bundle will be restored or whether this bundle is
 * considered expired and a new bundle will be used.
 */
@Suppress("TooManyFunctions")
class SessionBundleStorage(
    private val context: Context,
    private val engine: Engine,
    internal val bundleLifetime: Pair<Long, TimeUnit>
) : AutoSave.Storage {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var databaseInitializer = {
        BundleDatabase.get(context)
    }

    private val database by lazy { databaseInitializer() }
    private var lastBundle: BundleEntity? = null

    /**
     * Restores the last [SessionBundle] if there is one without expired lifetime.
     */
    @Synchronized
    @WorkerThread
    fun restore(): SessionBundle? {
        val since = now() - bundleLifetime.second.toMillis(bundleLifetime.first)

        val entity = database
            .bundleDao()
            .getLastBundle(since)
            .also { lastBundle = it }

        return entity?.let { SessionBundleAdapter(context, engine, it) }
    }

    /**
     * Saves the [SessionManager.Snapshot] as a bundle. If a bundle was restored previously then this bundle will be
     * updated with the data from the snapshot. If no bundle was restored a new bundle will be created.
     */
    @Synchronized
    @WorkerThread
    override fun save(snapshot: SessionManager.Snapshot): Boolean {
        val bundle = lastBundle

        if (bundle == null) {
            saveNewBundle(snapshot)
        } else {
            updateExistingBundle(bundle, snapshot)
        }

        return true
    }

    /**
     * Removes the given [SessionBundle] permanently. If this is the active bundle then a new one will be created the
     * next time a [SessionManager.Snapshot] is saved.
     */
    @Synchronized
    @WorkerThread
    fun remove(bundle: SessionBundle) {
        if (bundle !is SessionBundleAdapter) {
            throw IllegalArgumentException("Unexpected bundle type")
        }

        if (bundle.actual == lastBundle) {
            lastBundle = null
        }

        bundle.actual.let { database.bundleDao().deleteBundle(it) }

        bundle.actual.stateFile(context, engine).delete()
    }

    /**
     * Removes all saved [SessionBundle] instances permanently.
     */
    @Synchronized
    @WorkerThread
    fun removeAll() {
        lastBundle = null
        database.clearAllTables()

        getStateDirectory(context)
            .truncateDirectory()
    }

    /**
     * Returns the currently used [SessionBundle] for saving [SessionManager.Snapshot] instances. Or null if no bundle
     * is in use currently.
     */
    @Synchronized
    fun current(): SessionBundle? {
        return lastBundle?.let { SessionBundleAdapter(context, engine, it) }
    }

    /**
     * Explicitly uses the given [SessionBundle] (even if not active) to save [SessionManager.Snapshot] instances to.
     */
    @Synchronized
    fun use(bundle: SessionBundle) {
        if (bundle !is SessionBundleAdapter) {
            throw IllegalArgumentException("Unexpected bundle type")
        }

        lastBundle = bundle.actual
    }

    /**
     * Clear the currently used, active [SessionBundle] and use a new one the next time a [SessionManager.Snapshot] is
     * saved.
     */
    @Synchronized
    fun new() {
        lastBundle = null
    }

    /**
     * Returns the last saved [SessionBundle] instances (up to [limit]) as a [LiveData] list.
     *
     * @param since (Optional) Unix timestamp (milliseconds). If set this method will only return [SessionBundle]
     * instances that have been saved since the given timestamp.
     * @param limit (Optional) Maximum number of [SessionBundle] instances that should be returned.
     */
    fun bundles(since: Long = 0, limit: Int = 20): LiveData<List<SessionBundle>> {
        return Transformations.map(
            database
            .bundleDao()
            .getBundles(since, limit)
        ) { list ->
            list.map { SessionBundleAdapter(context, engine, it) }
        }
    }

    /**
     * Returns all saved [SessionBundle] instances as a [DataSource.Factory].
     *
     * A consuming app can transform the data source into a `LiveData<PagedList>` of when using RxJava2 into a
     * `Flowable<PagedList>` or `Observable<PagedList>`, that can be observed.
     *
     * - https://developer.android.com/topic/libraries/architecture/paging/data
     * - https://developer.android.com/topic/libraries/architecture/paging/ui
     *
     * @param since (Optional) Unix timestamp (milliseconds). If set this method will only return [SessionBundle]
     * instances that have been saved since the given timestamp.
     */
    fun bundlesPaged(since: Long = 0): DataSource.Factory<Int, SessionBundle> {
        return database
            .bundleDao()
            .getBundlesPaged(since)
            .map { entity -> SessionBundleAdapter(context, engine, entity) }
    }

    /**
     * Starts configuring automatic saving of the state.
     */
    @CheckResult
    fun autoSave(
        sessionManager: SessionManager,
        interval: Long = AutoSave.DEFAULT_INTERVAL_MILLISECONDS,
        unit: TimeUnit = TimeUnit.MILLISECONDS
    ): AutoSave {
        return AutoSave(sessionManager, this, unit.toMillis(interval))
    }

    /**
     * Automatically clears the current bundle and starts a new bundle if the lifetime has expired while the app was in
     * the background.
     */
    @Synchronized
    fun autoClose(
        sessionManager: SessionManager
    ) {
        ProcessLifecycleOwner.get().lifecycle.addObserver(
            SessionBundleLifecycleObserver(this, sessionManager))
    }

    /**
     * Save the given [SessionManager.Snapshot] as a new bundle.
     */
    private fun saveNewBundle(snapshot: SessionManager.Snapshot) {
        if (snapshot.isEmpty()) {
            // There's no need to save a new empty bundle to the database. Let's wait until there are sessions
            // in the snapshot.
            return
        }

        val bundle = snapshot.toBundleEntity().also {
            lastBundle = it
        }

        bundle.stateFile(context, engine).writeSnapshot(snapshot)

        bundle.id = database.bundleDao().insertBundle(bundle)
    }

    /**
     * Update the given bundle with the data from the [SessionManager.Snapshot].
     */
    private fun updateExistingBundle(bundle: BundleEntity, snapshot: SessionManager.Snapshot) {
        if (snapshot.isEmpty()) {
            // If this snapshot is empty then instead of saving an empty bundle: Remove the bundle. Otherwise
            // we end up with empty bundles to restore and that is not helpful at all.
            remove(SessionBundleAdapter(context, engine, bundle))
        } else {
            bundle.stateFile(context, engine).writeSnapshot(snapshot)

            bundle.updateFrom(snapshot)
            database.bundleDao().updateBundle(bundle)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun now() = System.currentTimeMillis()

    companion object {
        internal fun getStateDirectory(context: Context): File {
            return File(context.filesDir, "mozac.feature.session.bundling").apply {
                mkdirs()
            }
        }
    }
}
