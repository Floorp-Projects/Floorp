/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import android.graphics.Bitmap
import java.util.UUID

/**
 * Value type that represents the state of a tab (private or normal).
 *
 * @property id the ID of this tab and session.
 * @property content the [ContentState] of this tab.
 * @property trackingProtection the [TrackingProtectionState] of this tab.
 * @property parentId the parent ID of this tab or null if this tab has no
 * parent. The parent tab is usually the tab that initiated opening this
 * tab (e.g. the user clicked a link with target="_blank" or selected
 * "open in new tab" or a "window.open" was triggered).
 * @property extensionState a map of web extension ids to extensions,
 * that contains the overridden values for this tab.
 * @property readerState the [ReaderState] of this tab.
 * @property contextId the session context ID of this tab.
 */
data class TabSessionState(
    override val id: String = UUID.randomUUID().toString(),
    override val content: ContentState,
    override val trackingProtection: TrackingProtectionState = TrackingProtectionState(),
    override val engineState: EngineState = EngineState(),
    override val extensionState: Map<String, WebExtensionState> = emptyMap(),
    override val contextId: String? = null,
    override val source: SessionState.Source = SessionState.Source.NONE,
    override val crashed: Boolean = false,
    val parentId: String? = null,
    val lastAccess: Long = 0L,
    val readerState: ReaderState = ReaderState()
) : SessionState {

    override fun createCopy(
        id: String,
        content: ContentState,
        trackingProtection: TrackingProtectionState,
        engineState: EngineState,
        extensionState: Map<String, WebExtensionState>,
        contextId: String?,
        crashed: Boolean
    ): SessionState = copy(
        id = id,
        content = content,
        trackingProtection = trackingProtection,
        engineState = engineState,
        extensionState = extensionState,
        contextId = contextId,
        crashed = crashed
    )
}

/**
 * Convenient function for creating a tab.
 */
@Suppress("LongParameterList")
fun createTab(
    url: String,
    private: Boolean = false,
    id: String = UUID.randomUUID().toString(),
    parent: TabSessionState? = null,
    extensions: Map<String, WebExtensionState> = emptyMap(),
    readerState: ReaderState = ReaderState(),
    title: String = "",
    thumbnail: Bitmap? = null,
    contextId: String? = null,
    lastAccess: Long = 0L,
    source: SessionState.Source = SessionState.Source.NONE
): TabSessionState {
    return TabSessionState(
        id = id,
        content = ContentState(
            url,
            private,
            title = title,
            thumbnail = thumbnail
        ),
        parentId = parent?.id,
        extensionState = extensions,
        readerState = readerState,
        contextId = contextId,
        lastAccess = lastAccess,
        source = source
    )
}
