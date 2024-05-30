/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.session.SessionUseCases
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import org.mockito.Mockito.spy

class ToolbarInteractorTest {

    class TestToolbar : Toolbar {
        override var highlight: Toolbar.Highlight = Toolbar.Highlight.NONE
        override var url: CharSequence = ""
        override var siteSecure: Toolbar.SiteSecurity = Toolbar.SiteSecurity.INSECURE
        override var private: Boolean = false
        override var title: String = ""

        override var siteTrackingProtection: Toolbar.SiteTrackingProtection =
            Toolbar.SiteTrackingProtection.OFF_GLOBALLY

        override fun setSearchTerms(searchTerms: String) {
            fail()
        }

        override fun displayProgress(progress: Int) {
            fail()
        }

        override fun onBackPressed(): Boolean {
            fail()
            return false
        }

        override fun onStop() {
            fail()
        }

        override fun setOnUrlCommitListener(listener: (String) -> Boolean) {
            listener("https://mozilla.org")
        }

        override fun setAutocompleteListener(filter: suspend (String, AutocompleteDelegate) -> Unit) {
            fail()
        }

        override fun addBrowserAction(action: Toolbar.Action) {
            fail()
        }

        override fun removeBrowserAction(action: Toolbar.Action) {
            fail()
        }

        override fun removePageAction(action: Toolbar.Action) {
            fail()
        }

        override fun addPageAction(action: Toolbar.Action) {
            fail()
        }

        override fun addNavigationAction(action: Toolbar.Action) {
            fail()
        }

        override fun removeNavigationAction(action: Toolbar.Action) {
            fail()
        }

        override fun setOnEditListener(listener: Toolbar.OnEditListener) {
            fail()
        }

        override fun displayMode() {
            fail()
        }

        override fun editMode(cursorPlacement: Toolbar.CursorPlacement) {
            fail()
        }

        override fun addEditActionStart(action: Toolbar.Action) {
            fail()
        }

        override fun addEditActionEnd(action: Toolbar.Action) {
            fail()
        }

        override fun removeEditActionEnd(action: Toolbar.Action) {
            fail()
        }

        override fun hideMenuButton() {
            fail()
        }

        override fun showMenuButton() {
            fail()
        }

        override fun setDisplayHorizontalPadding(horizontalPadding: Int) {
            fail()
        }

        override fun invalidateActions() {
            fail()
        }

        override fun dismissMenu() {
            fail()
        }

        override fun enableScrolling() {
            fail()
        }

        override fun disableScrolling() {
            fail()
        }

        override fun collapse() {
            fail()
        }

        override fun expand() {
            fail()
        }
    }

    @Test
    fun `provide custom use case for loading url`() {
        var useCaseInvokedWithUrl = ""
        val loadUrlUseCase = object : SessionUseCases.LoadUrlUseCase {
            override fun invoke(
                url: String,
                flags: EngineSession.LoadUrlFlags,
                additionalHeaders: Map<String, String>?,
            ) {
                useCaseInvokedWithUrl = url
            }
        }

        val toolbarInteractor = spy(ToolbarInteractor(TestToolbar(), loadUrlUseCase))
        toolbarInteractor.start()

        assertEquals("https://mozilla.org", useCaseInvokedWithUrl)
    }
}
