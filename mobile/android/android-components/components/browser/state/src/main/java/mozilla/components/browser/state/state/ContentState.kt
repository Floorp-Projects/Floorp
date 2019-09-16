/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import android.graphics.Bitmap
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.prompt.PromptRequest

/**
 * Value type that represents the state of the content within a [SessionState].
 *
 * @property url the loading or loaded URL.
 * @property private whether or not the session is private.
 * @property title the title of the current page.
 * @property progress the loading progress of the current page.
 * @property searchTerms the last used search terms, or an empty string if no
 * search was executed for this session.
 * @property securityInfo the security information as [SecurityInfoState],
 * describing whether or not the this session is for a secure URL, as well
 * as the host and SSL certificate authority.
 * @property thumbnail the last generated [Bitmap] of this session's content, to
 * be used as a preview in e.g. a tab switcher.
 * @property icon the icon of the page currently loaded by this session.
 * @property download Last unhandled download request.
 * @property hitResult the target of the latest long click operation.
 * @property promptRequest the last received [PromptRequest].
 * @property findResults the list of results of the latest "find in page" operation.
 */
data class ContentState(
    val url: String,
    val private: Boolean = false,
    val title: String = "",
    val progress: Int = 0,
    val loading: Boolean = false,
    val searchTerms: String = "",
    val securityInfo: SecurityInfoState = SecurityInfoState(),
    val thumbnail: Bitmap? = null,
    val icon: Bitmap? = null,
    val download: DownloadState? = null,
    val hitResult: HitResult? = null,
    val promptRequest: PromptRequest? = null,
    val findResults: List<FindResultState> = emptyList()
)
