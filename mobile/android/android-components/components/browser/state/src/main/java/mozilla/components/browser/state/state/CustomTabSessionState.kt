/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.manifest.WebAppManifest
import java.util.UUID

/**
 * Value type that represents the state of a Custom Tab.
 *
 * @property id the ID of this custom tab and session.
 * @property content the [ContentState] of this custom tab.
 * @property trackingProtection the [TrackingProtectionState] of this custom tab.
 * @property config the [CustomTabConfig] used to create this custom tab.
 * @property extensionState a map of web extension ids and extensions, that contains the overridden
 * values for this tab.
 * @property mediaSessionState the [MediaSessionState] of this session.
 * @property contextId the session context ID of this custom tab.
 * @property source the [SessionState.Source] of this session.
 */
data class CustomTabSessionState(
    override val id: String = UUID.randomUUID().toString(),
    override val content: ContentState,
    override val trackingProtection: TrackingProtectionState = TrackingProtectionState(),
    val config: CustomTabConfig,
    override val engineState: EngineState = EngineState(),
    override val extensionState: Map<String, WebExtensionState> = emptyMap(),
    override val mediaSessionState: MediaSessionState? = null,
    override val contextId: String? = null,
    override val source: SessionState.Source = SessionState.Source.Internal.CustomTab,
    override val restored: Boolean = false,
) : SessionState {

    override fun createCopy(
        id: String,
        content: ContentState,
        trackingProtection: TrackingProtectionState,
        engineState: EngineState,
        extensionState: Map<String, WebExtensionState>,
        mediaSessionState: MediaSessionState?,
        contextId: String?,
    ) = copy(
        id = id,
        content = content,
        trackingProtection = trackingProtection,
        engineState = engineState,
        extensionState = extensionState,
        mediaSessionState = mediaSessionState,
        contextId = contextId,
    )
}

/**
 * Convenient function for creating a custom tab.
 */
@Suppress("LongParameterList")
fun createCustomTab(
    url: String,
    id: String = UUID.randomUUID().toString(),
    config: CustomTabConfig = CustomTabConfig(),
    title: String = "",
    contextId: String? = null,
    engineSession: EngineSession? = null,
    mediaSessionState: MediaSessionState? = null,
    crashed: Boolean = false,
    source: SessionState.Source = SessionState.Source.Internal.CustomTab,
    private: Boolean = false,
    webAppManifest: WebAppManifest? = null,
    initialLoadFlags: EngineSession.LoadUrlFlags = EngineSession.LoadUrlFlags.none(),
): CustomTabSessionState {
    return CustomTabSessionState(
        id = id,
        source = source,
        content = ContentState(
            url = url,
            title = title,
            private = private,
            webAppManifest = webAppManifest,
        ),
        config = config,
        mediaSessionState = mediaSessionState,
        contextId = contextId,
        engineState = EngineState(
            engineSession = engineSession,
            crashed = crashed,
            initialLoadFlags = initialLoadFlags,
        ),
    )
}
