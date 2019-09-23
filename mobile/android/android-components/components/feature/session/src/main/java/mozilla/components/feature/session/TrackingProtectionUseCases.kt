/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.content.blocking.TrackerLog
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
        FetchTrackingLogUserCase(
            sessionManager,
            engine
        )
    }
}
