/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.state

import mozilla.components.browser.session.store.BrowserStore
import mozilla.components.concept.engine.HitResult
import java.util.UUID

/**
 * Value type that represents the state of a browser session ("tab").
 *
 * [SessionState] instances are immutable and state changes are communicated as new state objects emitted by the
 * [BrowserStore].
 *
 * @property url The URL of the currently loading or loaded resource/page.
 * @property private Whether or not this session is in private mode (automatically erasing browsing information,
 * such as passwords, cookies and history, leaving no trace after the end of the session).
 * @property id A unique ID identifying this session.
 * @property source The source that created this session (e.g. a VIEW intent).
 * @property title The title of the currently displayed page (or an empty String).
 * @property progress The loading progress of the current page.
 * @property canGoBack Navigation state, true if there's an history item to go back to, otherwise false.
 * @property canGoForward Navigation state, true if there's an history item to go forward to, otherwise false.
 * @property searchTerms The search terms used for the currently loaded page (or an empty string) if this page was
 * loaded by performing a search.
 * @property securityInfo security information indicating whether or not the current session is for a secure URL, as
 * well as the host and SSL certificate authority, if applicable.
 * @property download A download request triggered by this session (or null).
 * @property trackerBlockingEnabled Tracker blocking state, true if blocking trackers is enabled, otherwise false.
 * @property trackersBlocked List of URIs that have been blocked on the currently displayed page.
 * @property hitResult The target of the latest long click operation on the currently displayed page.
 */
data class SessionState(
    val url: String,
    val private: Boolean = false,
    val id: String = UUID.randomUUID().toString(),
    val source: SourceState = SourceState.NONE,
    val title: String = "",
    val progress: Int = 0,
    val loading: Boolean = false,
    val canGoBack: Boolean = false,
    val canGoForward: Boolean = false,
    val searchTerms: String = "",
    val securityInfo: SecurityInfoState = SecurityInfoState(),
    val download: DownloadState? = null,
    val trackerBlockingEnabled: Boolean = false,
    val trackersBlocked: List<String> = emptyList(),
    val hitResult: HitResult? = null
)
