/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class LocaleAwareAppCompatActivityTest {

    @Test
    @Config(sdk = [Build.VERSION_CODES.N, Build.VERSION_CODES.P])
    fun `when version is not Android 8 don't set layoutDirection`() {
        val activity = spy(Robolectric.buildActivity(LocaleAwareAppCompatActivity::class.java).get())
        activity.setLayoutDirectionIfNeeded()
        verify(activity, Mockito.times(0)).window
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.O, Build.VERSION_CODES.O_MR1])
    fun `when version is Android 8 set layoutDirection`() {
        val activity = spy(Robolectric.buildActivity(LocaleAwareAppCompatActivity::class.java).get())
        activity.setLayoutDirectionIfNeeded()
        verify(activity, Mockito.times(1)).window
    }
}
