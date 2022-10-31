/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.content.Context
import android.os.Looper.getMainLooper
import android.view.View
import android.view.WindowManager
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.RelativeLayout
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import mozilla.components.support.base.android.Padding
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf
import org.robolectric.shadows.ShadowLooper
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class ViewTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `showKeyboard should request focus`() {
        val view = EditText(testContext)
        assertFalse(view.hasFocus())

        view.showKeyboard()
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks()

        assertTrue(view.hasFocus())
    }

    @Test
    fun `hideKeyboard should hide soft keyboard`() {
        val view = mock<View>()
        val context = mock<Context>()
        val imm = mock<InputMethodManager>()
        `when`(view.context).thenReturn(context)
        `when`(context.getSystemService(InputMethodManager::class.java)).thenReturn(imm)

        view.hideKeyboard()

        verify(imm).hideSoftInputFromWindow(view.windowToken, 0)
    }

    @Test
    fun `setPadding should set padding`() {
        val view = TextView(testContext)

        assertEquals(view.paddingLeft, 0)
        assertEquals(view.paddingTop, 0)
        assertEquals(view.paddingRight, 0)
        assertEquals(view.paddingBottom, 0)

        view.setPadding(Padding(16, 20, 24, 28))

        assertEquals(view.paddingLeft, 16)
        assertEquals(view.paddingTop, 20)
        assertEquals(view.paddingRight, 24)
        assertEquals(view.paddingBottom, 28)
    }

    @Test
    fun `getRectWithViewLocation should transform getLocationInWindow method values`() {
        val view = spy(View(testContext))
        doAnswer { invocation ->
            val locationInWindow = (invocation.getArgument(0) as IntArray)
            locationInWindow[0] = 100
            locationInWindow[1] = 200
            locationInWindow
        }.`when`(view).getLocationInWindow(any())

        `when`(view.width).thenReturn(150)
        `when`(view.height).thenReturn(250)

        val outRect = view.getRectWithViewLocation()

        assertEquals(100, outRect.left)
        assertEquals(200, outRect.top)
        assertEquals(250, outRect.right)
        assertEquals(450, outRect.bottom)
    }

    @Test
    fun `called after next layout`() {
        val view = View(testContext)

        var callbackInvoked = false
        view.onNextGlobalLayout {
            callbackInvoked = true
        }

        assertFalse(callbackInvoked)

        view.viewTreeObserver.dispatchOnGlobalLayout()

        assertTrue(callbackInvoked)
    }

    @Test
    fun `remove listener after next layout`() {
        val view = spy(View(testContext))
        val viewTreeObserver = spy(view.viewTreeObserver)
        doReturn(viewTreeObserver).`when`(view).viewTreeObserver

        view.onNextGlobalLayout {}

        verify(viewTreeObserver, never()).removeOnGlobalLayoutListener(any())

        viewTreeObserver.dispatchOnGlobalLayout()

        verify(viewTreeObserver).removeOnGlobalLayoutListener(any())
    }

    @Test
    fun `can dispatch coroutines to view scope`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val view = View(testContext)
        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))
        shadowOf(getMainLooper()).idle()

        assertTrue(view.isAttachedToWindow)

        val latch = CountDownLatch(1)
        var coroutineExecuted = false

        view.toScope().launch {
            coroutineExecuted = true
            latch.countDown()
        }

        latch.await(10, TimeUnit.SECONDS)

        assertTrue(coroutineExecuted)
    }

    @Test
    fun `scope is cancelled when view is detached`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val view = View(testContext)
        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))
        shadowOf(getMainLooper()).idle()

        val scope = view.toScope()

        assertTrue(view.isAttachedToWindow)
        assertTrue(scope.isActive)

        activity.windowManager.removeView(view)
        shadowOf(getMainLooper()).idle()

        assertFalse(view.isAttachedToWindow)
        assertFalse(scope.isActive)

        val latch = CountDownLatch(1)
        var coroutineExecuted = false

        scope.launch {
            coroutineExecuted = true
            latch.countDown()
        }

        assertFalse(latch.await(5, TimeUnit.SECONDS))
        assertFalse(coroutineExecuted)
    }

    @Test
    fun `correct view is found in the hierarchy matching the predicate`() {
        val root = LinearLayout(testContext)
        val layout = RelativeLayout(testContext)
        val testView = TestView(testContext)

        layout.addView(testView)
        root.addView(layout)

        val rootFound = root.findViewInHierarchy { it is LinearLayout }

        assertNotNull(rootFound)
        assertTrue(rootFound is LinearLayout)

        val layoutFound = root.findViewInHierarchy { it is RelativeLayout }

        assertNotNull(layoutFound)
        assertTrue(layoutFound is RelativeLayout)

        val testViewFound = root.findViewInHierarchy { it is TestView }

        assertNotNull(testViewFound)
        assertTrue(testViewFound is TestView)
    }

    private class TestView(context: Context) : View(context)
}
