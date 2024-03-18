/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl

/**
 * Connects a toolbar instance to the browser engine via use cases
 */
class ToolbarInteractor(
    private val toolbar: Toolbar,
    private val loadUrlUseCase: SessionUseCases.LoadUrlUseCase,
    private val searchUseCase: SearchUseCase? = null,
) {

    /**
     * Starts this interactor. Makes sure this interactor is listening
     * to relevant UI changes and triggers the corresponding use-cases
     * in response.
     */
    fun start() {
        toolbar.setOnUrlCommitListener { text ->
            when {
                text.isUrl() -> loadUrlUseCase.invoke(text.toNormalizedUrl())
                searchUseCase != null -> searchUseCase.invoke(text)
                else -> loadUrlUseCase.invoke(text)
            }
            true
        }
    }
}
