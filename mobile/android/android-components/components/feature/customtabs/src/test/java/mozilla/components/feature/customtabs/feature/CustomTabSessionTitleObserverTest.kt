/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.feature

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.concept.toolbar.AutocompleteDelegate
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

@RunWith(AndroidJUnit4::class)
class CustomTabSessionTitleObserverTest {

    @Test
    fun `show title only if not empty`() {
        val toolbar: Toolbar = mock()
        val session = Mockito.spy(Session("https://mozilla.org"))
        val observer = CustomTabSessionTitleObserver(toolbar)
        val title = "Internet for people, not profit - Mozilla"

        observer.onTitleChanged(session, title = "")
        verify(toolbar, never()).title = ""

        observer.onTitleChanged(session, title = title)
        verify(toolbar).title = title
    }

    @Test
    fun `Will use URL as title if title was shown once and is now empty`() {
        val toolbar = MockToolbar()
        val session = Session("https://mozilla.org")
        session.register(CustomTabSessionTitleObserver(toolbar))

        assertEquals("", toolbar.title)

        session.url = "https://www.mozilla.org/en-US/firefox/"
        assertEquals("", toolbar.title)

        session.title = "Firefox - Protect your life online with privacy-first products"
        assertEquals("Firefox - Protect your life online with privacy-first products", toolbar.title)

        session.url = "https://github.com/mozilla-mobile/android-components"
        assertEquals("Firefox - Protect your life online with privacy-first products", toolbar.title)

        session.title = ""
        assertEquals("https://github.com/mozilla-mobile/android-components", toolbar.title)

        session.title = "A collection of Android libraries to build browsers or browser-like applications."
        assertEquals("A collection of Android libraries to build browsers or browser-like applications.", toolbar.title)

        session.title = ""
        assertEquals("https://github.com/mozilla-mobile/android-components", toolbar.title)
    }

    private class MockToolbar : Toolbar {
        override var title: String = ""

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
        override fun addEditAction(action: Toolbar.Action) = Unit
        override fun setOnEditListener(listener: Toolbar.OnEditListener) = Unit
        override fun displayMode() = Unit
        override fun editMode() = Unit
    }

    private class ThrowProperty<T> : ReadWriteProperty<Any, T> {
        override fun getValue(thisRef: Any, property: KProperty<*>): T =
            throw UnsupportedOperationException("Cannot get $property")

        override fun setValue(thisRef: Any, property: KProperty<*>, value: T) =
            throw UnsupportedOperationException("Cannot set $property")
    }
}
