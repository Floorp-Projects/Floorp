/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.Settings

/**
 * Contains use cases related to user settings.
 *
 * @param settings the engine's [Settings].
 * @param sessionManager the application's [SessionManager].
 */
class SettingsUseCases(
        settings: Settings,
        sessionManager: SessionManager
) {

    class UpdateTrackingProtectionUseCase internal constructor(
            private val settings: Settings,
            private val sessionManager: SessionManager
    ) {

        /**
         * Updates the tracking protection policy to the given policy value.
         * All active sessions are automatically updated with the new policy.
         *
         * @param policy The new policy describing what Tracking Protection to use.
         */
        fun invoke(policy: TrackingProtectionPolicy) {
            settings.trackingProtectionPolicy = policy

            with(sessionManager) {
                sessions.forEach {
                    getEngineSession(it)?.enableTrackingProtection(policy)
                }
            }
        }
    }

    val updateTrackingProtection: UpdateTrackingProtectionUseCase by lazy { UpdateTrackingProtectionUseCase(settings, sessionManager) }
}
