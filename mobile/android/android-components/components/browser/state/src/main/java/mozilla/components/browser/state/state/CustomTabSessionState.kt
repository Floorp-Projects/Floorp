/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

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
 * @property contextId the session context ID of this custom tab.
 */
data class CustomTabSessionState(
    override val id: String = UUID.randomUUID().toString(),
    override val content: ContentState,
    override val trackingProtection: TrackingProtectionState = TrackingProtectionState(),
    val config: CustomTabConfig,
    override val engineState: EngineState = EngineState(),
    override val extensionState: Map<String, WebExtensionState> = emptyMap(),
    override val contextId: String? = null,
    override val source: SessionState.Source = SessionState.Source.CUSTOM_TAB
) : SessionState {

    override fun createCopy(
        id: String,
        content: ContentState,
        trackingProtection: TrackingProtectionState,
        engineState: EngineState,
        extensionState: Map<String, WebExtensionState>,
        contextId: String?
    ) = copy(
        id = id,
        content = content,
        trackingProtection = trackingProtection,
        engineState = engineState,
        extensionState = extensionState,
        contextId = contextId
    )
}

/**
 * Convenient function for creating a custom tab.
 */
fun createCustomTab(
    url: String,
    id: String = UUID.randomUUID().toString(),
    config: CustomTabConfig = CustomTabConfig(),
    contextId: String? = null
): CustomTabSessionState {
    return CustomTabSessionState(
        id = id,
        content = ContentState(url),
        config = config,
        contextId = contextId
    )
}
