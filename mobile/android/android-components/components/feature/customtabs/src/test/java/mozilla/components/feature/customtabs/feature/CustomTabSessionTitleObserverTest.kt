/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.feature

import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.ThrowProperty
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class CustomTabSessionTitleObserverTest {

    @Test
    fun `show title only if not empty`() {
        val toolbar: Toolbar = mock()
        val observer = CustomTabSessionTitleObserver(toolbar)
        val url = "https://www.mozilla.org"
        val title = "Internet for people, not profit - Mozilla"

        observer.onTab(createCustomTab(url, title = ""))
        verify(toolbar, never()).title = ""

        observer.onTab(createCustomTab(url, title = title))
        verify(toolbar).title = title
    }

    @Test
    fun `Will use URL as title if title was shown once and is now empty`() {
        val toolbar = MockToolbar()
        var tab = createCustomTab("https://mozilla.org")
        val observer = CustomTabSessionTitleObserver(toolbar)

        observer.onTab(tab)
        assertEquals("", toolbar.title)

        tab = tab.withUrl("https://www.mozilla.org/en-US/firefox/")
        observer.onTab(tab)
        assertEquals("", toolbar.title)

        tab = tab.withTitle("Firefox - Protect your life online with privacy-first products")
        observer.onTab(tab)
        assertEquals("Firefox - Protect your life online with privacy-first products", toolbar.title)

        tab = tab.withUrl("https://github.com/mozilla-mobile/android-components")
        observer.onTab(tab)
        assertEquals("Firefox - Protect your life online with privacy-first products", toolbar.title)

        tab = tab.withTitle("")
        observer.onTab(tab)
        assertEquals("https://github.com/mozilla-mobile/android-components", toolbar.title)

        tab = tab.withTitle("A collection of Android libraries to build browsers or browser-like applications.")
        observer.onTab(tab)
        assertEquals("A collection of Android libraries to build browsers or browser-like applications.", toolbar.title)

        tab = tab.withTitle("")
        observer.onTab(tab)
        assertEquals("https://github.com/mozilla-mobile/android-components", toolbar.title)
    }

    private class MockToolbar : Toolbar {
        override var title: String = ""
        override var highlight: Toolbar.Highlight = Toolbar.Highlight.NONE
        override var url: CharSequence by ThrowProperty()
        override var private: Boolean by ThrowProperty()
        override var siteSecure: Toolbar.SiteSecurity by ThrowProperty()
        override var siteTrackingProtection: Toolbar.SiteTrackingProtection by ThrowProperty()
        override fun setSearchTerms(searchTerms: String) = Unit
        override fun displayProgress(progress: Int) = Unit
        override fun onBackPressed(): Boolean = false
        override fun onStop() = Unit
        override fun setOnUrlCommitListener(listener: (String) -> Boolean) = Unit
        override fun setAutocompleteListener(filter: suspend (String, AutocompleteDelegate) -> Unit) = Unit
        override fun addBrowserAction(action: Toolbar.Action) = Unit
        override fun removeBrowserAction(action: Toolbar.Action) = Unit
        override fun invalidateActions() = Unit
        override fun addPageAction(action: Toolbar.Action) = Unit
        override fun removePageAction(action: Toolbar.Action) = Unit
        override fun addNavigationAction(action: Toolbar.Action) = Unit
        override fun removeNavigationAction(action: Toolbar.Action) = Unit
        override fun addEditActionStart(action: Toolbar.Action) = Unit
        override fun addEditActionEnd(action: Toolbar.Action) = Unit
        override fun removeEditActionEnd(action: Toolbar.Action) = Unit
        override fun hideMenuButton() = Unit
        override fun showMenuButton() = Unit
        override fun setOnEditListener(listener: Toolbar.OnEditListener) = Unit
        override fun displayMode() = Unit
        override fun editMode(cursorPlacement: Toolbar.CursorPlacement) = Unit
        override fun dismissMenu() = Unit
        override fun enableScrolling() = Unit
        override fun disableScrolling() = Unit
        override fun collapse() = Unit
        override fun expand() = Unit
    }
}

private fun CustomTabSessionState.withTitle(title: String) = copy(
    content = content.copy(title = title),
)

private fun CustomTabSessionState.withUrl(url: String) = copy(
    content = content.copy(url = url),
)
