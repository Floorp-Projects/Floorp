/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.concept.base.images.ImageSaveRequest
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] implementation for handling [ContentAction.UpdateThumbnailAction] and storing
 * the thumbnail to the disk cache.
 */
class ThumbnailsMiddleware(
    private val thumbnailStorage: ThumbnailStorage,
) : Middleware<BrowserState, BrowserAction> {
    @Suppress("ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is TabListAction.RemoveAllNormalTabsAction -> {
                context.state.tabs.filterNot { it.content.private }.forEach { tab ->
                    thumbnailStorage.deleteThumbnail(tab.id, isPrivate = false)
                }
            }
            is TabListAction.RemoveAllPrivateTabsAction -> {
                context.state.tabs.filter { it.content.private }.forEach { tab ->
                    thumbnailStorage.deleteThumbnail(tab.id, isPrivate = true)
                }
            }
            is TabListAction.RemoveAllTabsAction -> {
                thumbnailStorage.clearThumbnails()
            }
            is TabListAction.RemoveTabAction -> {
                // Delete the tab screenshot from the storage when the tab is removed.
                val isPrivate = context.state.isTabIdPrivate(action.tabId)
                thumbnailStorage.deleteThumbnail(action.tabId, isPrivate)
            }
            is TabListAction.RemoveTabsAction -> {
                action.tabIds.forEach { id ->
                    val isPrivate = context.state.isTabIdPrivate(id)
                    thumbnailStorage.deleteThumbnail(id, isPrivate)
                }
            }
            is ContentAction.UpdateThumbnailAction -> {
                // Store the captured tab screenshot from the EngineView when the session's
                // thumbnail is updated.
                context.store.state.tabs.find { it.id == action.sessionId }?.let { session ->
                    val request = ImageSaveRequest(session.id, session.content.private)
                    thumbnailStorage.saveThumbnail(request, action.thumbnail)
                }
                return // Do not let the thumbnail actions continue through to the reducer.
            }
            else -> {
                // no-op
            }
        }
        next(action)
    }

    private fun BrowserState.isTabIdPrivate(id: String): Boolean =
        tabs.any { it.id == id && it.content.private }
}
