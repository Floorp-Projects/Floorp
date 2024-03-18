/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import androidx.core.net.toUri
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.support.base.log.logger.Logger
import java.lang.Exception

/**
 * Contains use cases related to the tracking protection.
 *
 * @param store the application's [BrowserStore].
 * @param engine the application's [Engine].
 */
class TrackingProtectionUseCases(
    val store: BrowserStore,
    val engine: Engine,
) {

    /**
     * Use case for adding a new tab to the exception list.
     */
    class AddExceptionUseCase internal constructor(
        private val store: BrowserStore,
        private val engine: Engine,
    ) {
        private val logger = Logger("TrackingProtectionUseCases")

        /**
         * Adds a new tab to the exception list, as a result this tab will not get applied any
         * tracking protection policy.
         * @param tabId The id of the tab that will be added to the exception list.
         * @param persistInPrivateMode Indicates if the exception should be persistent in private mode
         * defaults to false.
         */
        operator fun invoke(tabId: String, persistInPrivateMode: Boolean = false) {
            val engineSession = store.state.findTabOrCustomTabOrSelectedTab(tabId)?.engineState?.engineSession
                ?: return logger.error("The engine session should not be null")

            engine.trackingProtectionExceptionStore.add(engineSession, persistInPrivateMode)
        }
    }

    /**
     * Use case for removing a tab or a [TrackingProtectionException] from the exception list.
     */
    class RemoveExceptionUseCase internal constructor(
        private val store: BrowserStore,
        private val engine: Engine,
    ) {
        private val logger = Logger("TrackingProtectionUseCases")

        /**
         * Removes a tab from the exception list.
         * @param tabId The id of the tab that will be removed from the exception list.
         */
        operator fun invoke(tabId: String) {
            val engineSession = store.state.findTabOrCustomTabOrSelectedTab(tabId)?.engineState?.engineSession
                ?: return logger.error("The engine session should not be null")

            engine.trackingProtectionExceptionStore.remove(engineSession)
        }

        /**
         * Removes a [exception] from the exception list.
         * @param exception The [TrackingProtectionException] that will be removed from the exception list.
         */
        operator fun invoke(exception: TrackingProtectionException) {
            engine.trackingProtectionExceptionStore.remove(exception)
            // Find all tabs that need to update their tracking protection status.
            val tabs = (store.state.tabs + store.state.customTabs).filter { tab ->
                val tabDomain = tab.content.url.toUri().host
                val exceptionDomain = exception.url.toUri().host
                tabDomain == exceptionDomain
            }
            tabs.forEach {
                store.dispatch(TrackingProtectionAction.ToggleExclusionListAction(it.id, false))
            }
        }
    }

    /**
     * Use case for removing all tabs from the exception list.
     */
    class RemoveAllExceptionsUseCase internal constructor(
        private val store: BrowserStore,
        private val engine: Engine,
    ) {
        /**
         * Removes all domains from the exception list.
         */
        operator fun invoke(onRemove: () -> Unit = {}) {
            val engineSessions = (store.state.tabs + store.state.customTabs).mapNotNull { tab ->
                tab.engineState.engineSession
            }

            engine.trackingProtectionExceptionStore.removeAll(engineSessions, onRemove)
        }
    }

    /**
     * Use case for verifying if a tab is in the exception list.
     */
    class ContainsExceptionUseCase internal constructor(
        private val store: BrowserStore,
        private val engine: Engine,
    ) {
        /**
         * Indicates if a given tab is in the exception list.
         * @param tabId The id of the tab to verify.
         * @param onResult A callback to inform if the given tab is on
         * the exception list, true if it is in otherwise false.
         */
        operator fun invoke(
            tabId: String,
            onResult: (Boolean) -> Unit,
        ) {
            val engineSession = store.state.findTabOrCustomTabOrSelectedTab(tabId)?.engineState?.engineSession
                ?: return onResult(false)

            engine.trackingProtectionExceptionStore.contains(engineSession, onResult)
        }
    }

    /**
     * Use case for fetching all exceptions in the exception list.
     */
    class FetchExceptionsUseCase internal constructor(
        private val engine: Engine,
    ) {
        /**
         * Fetch all domains that will be ignored for tracking protection.
         * @param onResult A callback to inform that the domains on the exception list has been fetched,
         * it provides a list of [TrackingProtectionException] that are on the exception list, if there are none domains
         * on the exception list, an empty list will be provided.
         */
        operator fun invoke(onResult: (List<TrackingProtectionException>) -> Unit) {
            engine.trackingProtectionExceptionStore.fetchAll(onResult)
        }
    }

    /**
     * Use case for fetching all the tracking protection logged information.
     */
    class FetchTrackingLogUserCase internal constructor(
        private val store: BrowserStore,
        private val engine: Engine,
    ) {
        /**
         * Fetch all the tracking protection logged information of a given tab.
         *
         * @param tabId the id of the tab for which loading should be stopped.
         * @param onSuccess callback invoked if the data was fetched successfully.
         * @param onError (optional) callback invoked if fetching the data caused an exception.
         */
        operator fun invoke(
            tabId: String,
            onSuccess: (List<TrackerLog>) -> Unit,
            onError: (Throwable) -> Unit,
        ) {
            val engineSession = store.state.findTabOrCustomTabOrSelectedTab(tabId)?.engineState?.engineSession
                ?: return onError(Exception("The engine session should not be null"))

            engine.getTrackersLog(engineSession, onSuccess, onError)
        }
    }

    val fetchTrackingLogs: FetchTrackingLogUserCase by lazy {
        FetchTrackingLogUserCase(store, engine)
    }
    val addException: AddExceptionUseCase by lazy {
        AddExceptionUseCase(store, engine)
    }
    val removeException: RemoveExceptionUseCase by lazy {
        RemoveExceptionUseCase(store, engine)
    }
    val containsException: ContainsExceptionUseCase by lazy {
        ContainsExceptionUseCase(store, engine)
    }
    val removeAllExceptions: RemoveAllExceptionsUseCase by lazy {
        RemoveAllExceptionsUseCase(store, engine)
    }
    val fetchExceptions: FetchExceptionsUseCase by lazy {
        FetchExceptionsUseCase(engine)
    }
}
