/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] that updates [TabSessionState.lastMediaAccessState] everytime the user starts playing media or
 * the [MediaSession] gets deactivated as when the user navigates to other URL or starts playing media
 * in another tab.
 */
class LastMediaAccessMiddleware : Middleware<BrowserState, BrowserAction> {
    @Suppress("ComplexCondition")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        next(action)

        if (action is MediaSessionAction.UpdateMediaPlaybackStateAction &&
            action.playbackState == MediaSession.PlaybackState.PLAYING
        ) {
            context.dispatch(LastAccessAction.UpdateLastMediaAccessAction(action.tabId))
        } else if (action is MediaSessionAction.DeactivatedMediaSessionAction) {
            context.dispatch(LastAccessAction.ResetLastMediaSessionAction(action.tabId))
        }
    }
}
