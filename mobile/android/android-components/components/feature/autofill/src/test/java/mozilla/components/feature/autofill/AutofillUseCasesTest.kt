/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill

import android.content.Context
import android.view.autofill.AutofillManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AutofillUseCasesTest {
    @Test
    @Ignore("Requires updated Robolectric and Mockito with Java 11: https://github.com/mozilla-mobile/android-components/issues/10550")
    fun testIsSupported() {
        val context: Context = mock()
        val autofillManager: AutofillManager = mock()
        doReturn(autofillManager).`when`(context).getSystemService(AutofillManager::class.java)
        doReturn(true).`when`(autofillManager).isAutofillSupported

        assertFalse(AutofillUseCases(sdkVersion = 21).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 22).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 23).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 24).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 25).isSupported(context))

        assertTrue(AutofillUseCases(sdkVersion = 26).isSupported(context))
        assertTrue(AutofillUseCases(sdkVersion = 27).isSupported(context))
        assertTrue(AutofillUseCases(sdkVersion = 28).isSupported(context))
        assertTrue(AutofillUseCases(sdkVersion = 29).isSupported(context))
        assertTrue(AutofillUseCases(sdkVersion = 30).isSupported(context))
    }

    @Test
    @Ignore("Requires updated Robolectric and Mockito with Java 11: https://github.com/mozilla-mobile/android-components/issues/10550")
    fun testIsNotSupported() {
        val context: Context = mock()
        val autofillManager: AutofillManager = mock()
        doReturn(autofillManager).`when`(context).getSystemService(AutofillManager::class.java)
        doReturn(false).`when`(autofillManager).isAutofillSupported

        assertFalse(AutofillUseCases(sdkVersion = 21).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 22).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 23).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 24).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 25).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 26).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 27).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 28).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 29).isSupported(context))
        assertFalse(AutofillUseCases(sdkVersion = 30).isSupported(context))
    }

    @Test
    @Ignore("Requires updated Robolectric and Mockito with Java 11: https://github.com/mozilla-mobile/android-components/issues/10550")
    fun testIsEnabled() {
        val context: Context = mock()
        val autofillManager: AutofillManager = mock()
        doReturn(autofillManager).`when`(context).getSystemService(AutofillManager::class.java)
        doReturn(true).`when`(autofillManager).hasEnabledAutofillServices()

        assertFalse(AutofillUseCases(sdkVersion = 21).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 22).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 23).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 24).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 25).isEnabled(context))

        assertTrue(AutofillUseCases(sdkVersion = 26).isEnabled(context))
        assertTrue(AutofillUseCases(sdkVersion = 27).isEnabled(context))
        assertTrue(AutofillUseCases(sdkVersion = 28).isEnabled(context))
        assertTrue(AutofillUseCases(sdkVersion = 29).isEnabled(context))
        assertTrue(AutofillUseCases(sdkVersion = 30).isEnabled(context))
    }

    @Test
    @Ignore("Requires updated Robolectric and Mockito with Java 11: https://github.com/mozilla-mobile/android-components/issues/10550")
    fun testIsNotEnabled() {
        val context: Context = mock()
        val autofillManager: AutofillManager = mock()
        doReturn(autofillManager).`when`(context).getSystemService(AutofillManager::class.java)
        doReturn(false).`when`(autofillManager).hasEnabledAutofillServices()

        assertFalse(AutofillUseCases(sdkVersion = 21).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 22).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 23).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 24).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 25).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 26).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 27).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 28).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 29).isEnabled(context))
        assertFalse(AutofillUseCases(sdkVersion = 30).isEnabled(context))
    }

    @Test
    fun testEnable() {
        val context: Context = mock()

        AutofillUseCases(sdkVersion = 21).enable(context)
        verify(context, never()).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 22).enable(context)
        verify(context, never()).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 23).enable(context)
        verify(context, never()).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 24).enable(context)
        verify(context, never()).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 25).enable(context)
        verify(context, never()).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 26).enable(context)
        verify(context).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 27).enable(context)
        verify(context).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 28).enable(context)
        verify(context).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 29).enable(context)
        verify(context).startActivity(any())
        reset(context)

        AutofillUseCases(sdkVersion = 30).enable(context)
        verify(context).startActivity(any())
        reset(context)
    }

    @Test
    @Ignore("Requires updated Robolectric and Mockito with Java 11: https://github.com/mozilla-mobile/android-components/issues/10550")
    fun testDisable() {
        val context: Context = mock()
        val autofillManager: AutofillManager = mock()
        doReturn(autofillManager).`when`(context).getSystemService(AutofillManager::class.java)

        AutofillUseCases(sdkVersion = 21).disable(context)
        verify(autofillManager, never()).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 22).disable(context)
        verify(autofillManager, never()).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 23).disable(context)
        verify(autofillManager, never()).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 24).disable(context)
        verify(autofillManager, never()).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 25).disable(context)
        verify(autofillManager, never()).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 26).disable(context)
        verify(autofillManager).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 27).disable(context)
        verify(autofillManager).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 28).disable(context)
        verify(autofillManager).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 29).disable(context)
        verify(autofillManager).disableAutofillServices()
        reset(autofillManager)

        AutofillUseCases(sdkVersion = 30).disable(context)
        verify(autofillManager).disableAutofillServices()
        reset(autofillManager)
    }
}
