/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.support.base.log.logger.Logger
import java.lang.Exception

/**
 * Contains use cases related to the tracking protection.
 *
 * @param sessionManager the application's [SessionManager].
 * @param engine the application's [Engine].
 */
class TrackingProtectionUseCases(
    val sessionManager: SessionManager,
    val engine: Engine
) {

    /**
     * Use case for adding a new [Session] to the exception list.
     */
    class AddExceptionUseCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine
    ) {
        private val logger = Logger("TrackingProtectionUseCases")

        /**
         * Adds a new [session] to the exception list, as a result this session will
         * not get applied any tracking protection policy.
         * @param session The [session] that will be added to the exception list.
         */
        operator fun invoke(session: Session) {
            val engineSession = sessionManager.getEngineSession(session)
                ?: return logger.error("The engine session should not be null")

            engine.trackingProtectionExceptionStore.add(engineSession)
        }
    }

    /**
     * Use case for removing a [Session] from the exception list.
     */
    class RemoveExceptionUseCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine
    ) {
        private val logger = Logger("TrackingProtectionUseCases")

        /**
         * Removes a [session] from the exception list.
         * @param session The [session] that will be removed from the exception list.
         */
        operator fun invoke(session: Session) {
            val engineSession = sessionManager.getEngineSession(session)
                ?: return logger.error("The engine session should not be null")

            engine.trackingProtectionExceptionStore.remove(engineSession)
        }
    }

    /**
     * Use case for removing all [Session]s from the exception list.
     */
    class RemoveAllExceptionsUseCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine
    ) {
        /**
         * Removes all domains from the exception list.
         */
        operator fun invoke() {
            engine.trackingProtectionExceptionStore.removeAll()
        }
    }

    /**
     * Use case for verifying if a [Session] is in the exception list.
     */
    class ContainsExceptionUseCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine
    ) {
        /**
         * Indicates if a given [session] is in the exception list.
         * @param session The [session] to verify.
         * @param onResult A callback to inform if the given [session] is on
         * the exception list, true if it is in otherwise false.
         */
        operator fun invoke(
            session: Session,
            onResult: (Boolean) -> Unit
        ) {
            val engineSession = sessionManager.getEngineSession(session) ?: return onResult(false)
            engine.trackingProtectionExceptionStore.contains(engineSession, onResult)
        }
    }

    /**
     * Use case for fetching all exceptions in the exception list.
     */
    class FetchExceptionsUseCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine
    ) {
        /**
         * Fetch all domains that will be ignored for tracking protection.
         * @param onResult A callback to inform that the domains on the exception list has been fetched,
         * it provides a list of domains that are on the exception list, if there are none domains
         * on the exception list, an empty list will be provided.
         */
        operator fun invoke(onResult: (List<String>) -> Unit) {
            engine.trackingProtectionExceptionStore.fetchAll(onResult)
        }
    }

    /**
     * Use case for fetching all the tracking protection logged information.
     */
    class FetchTrackingLogUserCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine
    ) {
        /**
         * Fetch all the tracking protection logged information of a given [session].
         *
         * @param session the session for which loading should be stopped.
         * @param onSuccess callback invoked if the data was fetched successfully.
         * @param onError (optional) callback invoked if fetching the data caused an exception.
         */
        operator fun invoke(
            session: Session,
            onSuccess: (List<TrackerLog>) -> Unit,
            onError: (Throwable) -> Unit
        ) {
            val engineSession =
                sessionManager.getEngineSession(session)
                    ?: return onError(Exception("The engine session should not be null"))
            engine.getTrackersLog(engineSession, onSuccess, onError)
        }
    }

    val fetchTrackingLogs: FetchTrackingLogUserCase by lazy {
        FetchTrackingLogUserCase(sessionManager, engine)
    }
    val addException: AddExceptionUseCase by lazy {
        AddExceptionUseCase(sessionManager, engine)
    }
    val removeException: RemoveExceptionUseCase by lazy {
        RemoveExceptionUseCase(sessionManager, engine)
    }
    val containsException: ContainsExceptionUseCase by lazy {
        ContainsExceptionUseCase(sessionManager, engine)
    }
    val removeAllExceptions: RemoveAllExceptionsUseCase by lazy {
        RemoveAllExceptionsUseCase(sessionManager, engine)
    }
    val fetchExceptions: FetchExceptionsUseCase by lazy {
        FetchExceptionsUseCase(sessionManager, engine)
    }
}
