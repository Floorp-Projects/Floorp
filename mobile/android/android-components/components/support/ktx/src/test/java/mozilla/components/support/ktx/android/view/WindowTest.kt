/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.graphics.Color
import android.os.Build
import android.view.View
import android.view.Window
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations.openMocks
import org.robolectric.util.ReflectionHelpers.setStaticField
import kotlin.reflect.jvm.javaField

@RunWith(AndroidJUnit4::class)
class WindowTest {

    @Mock private lateinit var window: Window

    @Mock private lateinit var decorView: View

    @Before
    fun setup() {
        setStaticField(Build.VERSION::SDK_INT.javaField, 0)

        openMocks(this)

        `when`(window.decorView).thenAnswer { decorView }
    }

    @After
    fun teardown() = setStaticField(Build.VERSION::SDK_INT.javaField, 0)

    @Test
    fun `setStatusBarTheme changes status bar color`() {
        window.setStatusBarTheme(Color.RED)

        verify(window).statusBarColor = Color.RED
    }

    @Test
    fun `setNavigationBarTheme changes navigation bar color`() {
        window.setNavigationBarTheme(Color.RED)

        verify(window).navigationBarColor = Color.RED

        setStaticField(Build.VERSION::SDK_INT.javaField, Build.VERSION_CODES.P)
        window.setNavigationBarTheme(Color.BLUE)

        verify(window).navigationBarDividerColor = 0
        verify(window).navigationBarColor = Color.BLUE
    }
}
