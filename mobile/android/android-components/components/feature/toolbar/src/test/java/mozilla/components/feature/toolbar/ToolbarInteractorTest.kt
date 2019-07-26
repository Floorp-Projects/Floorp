/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.toolbar

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.session.SessionUseCases
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class ToolbarInteractorTest {

    class TestToolbar : Toolbar {
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

        override fun setOnUrlCommitListener(listener: (String) -> Boolean) {
            listener("https://mozilla.org")
        }

        override fun setAutocompleteListener(filter: suspend (String, AutocompleteDelegate) -> Unit) {
            fail()
        }

        override fun addBrowserAction(action: Toolbar.Action) {
            fail()
        }

        override fun addPageAction(action: Toolbar.Action) {
            fail()
        }

        override fun addNavigationAction(action: Toolbar.Action) {
            fail()
        }

        override fun setOnEditListener(listener: Toolbar.OnEditListener) {
            fail()
        }

        override fun displayMode() {
            fail()
        }

        override fun editMode() {
            fail()
        }

        override fun addEditAction(action: Toolbar.Action) {
            fail()
        }
    }

    @Test
    fun `provide custom use case for loading url`() {
        var useCaseInvokedWithUrl = ""
        val loadUrlUseCase = object : SessionUseCases.LoadUrlUseCase {
            override fun invoke(url: String, flags: EngineSession.LoadUrlFlags) {
                useCaseInvokedWithUrl = url
            }
        }

        val toolbarInteractor = spy(ToolbarInteractor(TestToolbar(), loadUrlUseCase))
        toolbarInteractor.start()

        assertEquals("https://mozilla.org", useCaseInvokedWithUrl)
    }
}
