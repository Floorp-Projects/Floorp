/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.support.v13.view.inputmethod.EditorInfoCompat
import android.util.AttributeSet
import android.view.View
import android.widget.ImageButton
import android.widget.LinearLayout
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.toolbar.BrowserToolbar.Companion.ACTION_PADDING_DP
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.browser.toolbar.edit.EditToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.concept.toolbar.Toolbar.SiteSecurity
import mozilla.components.support.base.android.Padding
import mozilla.components.support.ktx.android.view.isGone
import mozilla.components.support.ktx.android.view.isVisible
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserToolbarTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `display toolbar is visible by default`() {
        val toolbar = BrowserToolbar(context)
        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `calling editMode() makes edit toolbar visible`() {
        val toolbar = BrowserToolbar(context)
        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)

        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.visibility == View.GONE)
        assertTrue(toolbar.editToolbar.visibility == View.VISIBLE)
    }

    @Test
    fun `calling displayMode() makes display toolbar visible`() {
        val toolbar = BrowserToolbar(context)
        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.visibility == View.GONE)
        assertTrue(toolbar.editToolbar.visibility == View.VISIBLE)

        toolbar.displayMode()

        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `back presses will not be handled in display mode`() {
        val toolbar = BrowserToolbar(context)
        toolbar.displayMode()

        assertFalse(toolbar.onBackPressed())

        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `back presses will switch from edit mode to display mode`() {
        val toolbar = BrowserToolbar(context)
        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.visibility == View.GONE)
        assertTrue(toolbar.editToolbar.visibility == View.VISIBLE)

        assertTrue(toolbar.onBackPressed())

        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `displayUrl will be forwarded to display toolbar immediately`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = mock(DisplayToolbar::class.java)
        val ediToolbar = mock(EditToolbar::class.java)

        toolbar.displayToolbar = displayToolbar
        toolbar.editToolbar = ediToolbar

        toolbar.url = "https://www.mozilla.org"

        verify(displayToolbar).updateUrl("https://www.mozilla.org")
        verify(ediToolbar, never()).updateUrl(ArgumentMatchers.anyString())
    }

    @Test
    fun `last URL will be forwarded to edit toolbar when switching mode`() {
        val toolbar = BrowserToolbar(context)

        val ediToolbar = mock(EditToolbar::class.java)
        toolbar.editToolbar = ediToolbar

        toolbar.url = "https://www.mozilla.org"
        verify(ediToolbar, never()).updateUrl("https://www.mozilla.org")

        toolbar.editMode()

        verify(ediToolbar).updateUrl("https://www.mozilla.org")
    }

    @Test
    fun `displayProgress will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = mock(DisplayToolbar::class.java)

        toolbar.displayToolbar = displayToolbar

        toolbar.displayProgress(10)
        toolbar.displayProgress(50)
        toolbar.displayProgress(75)
        toolbar.displayProgress(100)

        verify(displayToolbar).updateProgress(10)
        verify(displayToolbar).updateProgress(50)
        verify(displayToolbar).updateProgress(75)
        verify(displayToolbar).updateProgress(100)

        verifyNoMoreInteractions(displayToolbar)
    }

    @Test
    fun `internal onUrlEntered callback will be forwarded to urlChangeListener`() {
        val toolbar = BrowserToolbar(context)

        val mockedListener = object {
            var called = false
            var url: String? = null

            fun invoke(url: String): Boolean {
                this.called = true
                this.url = url
                return true
            }
        }

        toolbar.setOnUrlCommitListener(mockedListener::invoke)
        toolbar.onUrlEntered("https://www.mozilla.org")

        assertTrue(mockedListener.called)
        assertEquals("https://www.mozilla.org", mockedListener.url)
    }

    @Test
    fun `internal onEditCancelled callback will be forwarded to editListener`() {
        val toolbar = BrowserToolbar(context)
        val listener: Toolbar.OnEditListener = mock()
        toolbar.setOnEditListener(listener)
        assertEquals(toolbar.editToolbar.editListener, listener)

        toolbar.onEditCancelled()
        verify(listener, times(1)).onCancelEditing()
    }

    @Test
    fun `toolbar measure will use full width and fixed 56dp height`() {
        val toolbar = BrowserToolbar(context)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.AT_MOST)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.AT_MOST)

        toolbar.measure(widthSpec, heightSpec)

        assertEquals(1024, toolbar.measuredWidth)
        assertEquals(56, toolbar.measuredHeight)
    }

    @Test
    fun `toolbar will use provided height with EXACTLY measure spec`() {
        val toolbar = BrowserToolbar(context)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.AT_MOST)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY)

        toolbar.measure(widthSpec, heightSpec)

        assertEquals(1024, toolbar.measuredWidth)
        assertEquals(800, toolbar.measuredHeight)
    }

    @Test
    fun `display and edit toolbar will use full size of browser toolbar`() {
        val toolbar = BrowserToolbar(context)

        assertEquals(0, toolbar.displayToolbar.measuredWidth)
        assertEquals(0, toolbar.displayToolbar.measuredHeight)
        assertEquals(0, toolbar.editToolbar.measuredWidth)
        assertEquals(0, toolbar.editToolbar.measuredHeight)

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1024, View.MeasureSpec.AT_MOST)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.AT_MOST)

        toolbar.measure(widthSpec, heightSpec)

        assertEquals(1024, toolbar.displayToolbar.measuredWidth)
        assertEquals(56, toolbar.displayToolbar.measuredHeight)
        assertEquals(1024, toolbar.editToolbar.measuredWidth)
        assertEquals(56, toolbar.editToolbar.measuredHeight)
    }

    @Test
    fun `toolbar will switch back to display mode after an URL has been entered`() {
        val toolbar = BrowserToolbar(context)
        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.visibility == View.GONE)
        assertTrue(toolbar.editToolbar.visibility == View.VISIBLE)

        toolbar.onUrlEntered("https://www.mozilla.org")

        assertTrue(toolbar.displayToolbar.visibility == View.VISIBLE)
        assertTrue(toolbar.editToolbar.visibility == View.GONE)
    }

    @Test
    fun `toolbar will switch back to display mode if URL commit listener returns true`() {
        val toolbar = BrowserToolbar(context)
        toolbar.setOnUrlCommitListener { true }
        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.isGone())
        assertTrue(toolbar.editToolbar.isVisible())

        toolbar.onUrlEntered("https://www.mozilla.org")

        assertTrue(toolbar.displayToolbar.isVisible())
        assertTrue(toolbar.editToolbar.isGone())
    }

    @Test
    fun `toolbar will stay in edit mode if URL commit listener returns false`() {
        val toolbar = BrowserToolbar(context)
        toolbar.setOnUrlCommitListener { false }
        toolbar.editMode()

        assertTrue(toolbar.displayToolbar.isGone())
        assertTrue(toolbar.editToolbar.isVisible())

        toolbar.onUrlEntered("https://www.mozilla.org")

        assertTrue(toolbar.displayToolbar.isGone())
        assertTrue(toolbar.editToolbar.isVisible())
    }

    @Test
    fun `display and edit toolbar will be laid out at the exact same position`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = mock(DisplayToolbar::class.java)
        val editToolbar = mock(EditToolbar::class.java)

        toolbar.displayToolbar = displayToolbar
        toolbar.editToolbar = editToolbar

        toolbar.removeAllViews()
        toolbar.addView(toolbar.displayToolbar)
        toolbar.addView(toolbar.editToolbar)

        `when`(displayToolbar.measuredWidth).thenReturn(1000)
        `when`(displayToolbar.measuredHeight).thenReturn(200)
        `when`(editToolbar.measuredWidth).thenReturn(1000)
        `when`(editToolbar.measuredHeight).thenReturn(200)

        toolbar.layout(100, 100, 1100, 300)

        verify(displayToolbar).layout(0, 0, 1000, 200)
        verify(editToolbar).layout(0, 0, 1000, 200)
    }

    @Test
    fun `menu builder will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(context)

        assertNull(toolbar.displayToolbar.menuBuilder)

        val menuBuilder = BrowserMenuBuilder(emptyList())
        toolbar.setMenuBuilder(menuBuilder)

        assertNotNull(toolbar.displayToolbar.menuBuilder)
        assertEquals(menuBuilder, toolbar.displayToolbar.menuBuilder)
    }

    @Test
    fun `add browser action will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = mock(DisplayToolbar::class.java)

        toolbar.displayToolbar = displayToolbar

        val action = BrowserToolbar.Button(mock(), "Hello") {
            // Do nothing
        }

        toolbar.addBrowserAction(action)

        verify(displayToolbar).addBrowserAction(action)
    }

    @Test
    fun `add page action will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(context)

        val displayToolbar = mock(DisplayToolbar::class.java)

        toolbar.displayToolbar = displayToolbar

        val action = BrowserToolbar.Button(mock(), "World") {
            // Do nothing
        }

        toolbar.addPageAction(action)

        verify(displayToolbar).addPageAction(action)
    }

    @Test
    fun `URL update does not override search terms in edit mode`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = mock(DisplayToolbar::class.java)
        val editToolbar = mock(EditToolbar::class.java)

        toolbar.displayToolbar = displayToolbar
        toolbar.editToolbar = editToolbar

        toolbar.setSearchTerms("mozilla android")
        toolbar.url = "https://www.mozilla.com"
        toolbar.editMode()
        verify(displayToolbar).updateUrl("https://www.mozilla.com")
        verify(editToolbar).updateUrl("mozilla android")

        toolbar.setSearchTerms("")
        toolbar.url = "https://www.mozilla.org"
        toolbar.editMode()
        verify(displayToolbar).updateUrl("https://www.mozilla.org")
        verify(editToolbar).updateUrl("https://www.mozilla.org")
    }

    @Test
    fun `add navigation action will be forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = mock(DisplayToolbar::class.java)
        toolbar.displayToolbar = displayToolbar

        val action = BrowserToolbar.Button(mock(), "Back") {
            // Do nothing
        }

        toolbar.addNavigationAction(action)

        verify(displayToolbar).addNavigationAction(action)
    }

    @Test
    fun `invalidate actions is forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = mock(DisplayToolbar::class.java)
        toolbar.displayToolbar = displayToolbar

        verify(displayToolbar, never()).invalidateActions()

        toolbar.invalidateActions()

        verify(displayToolbar).invalidateActions()
    }

    @Test
    fun `search terms (if set) are forwarded to edit toolbar instead of URL`() {
        val toolbar = BrowserToolbar(context)

        val ediToolbar = mock(EditToolbar::class.java)
        toolbar.editToolbar = ediToolbar

        toolbar.url = "https://www.mozilla.org"
        toolbar.setSearchTerms("Mozilla Firefox")

        verify(ediToolbar, never()).updateUrl("https://www.mozilla.org")
        verify(ediToolbar, never()).updateUrl("Mozilla Firefox")

        toolbar.editMode()

        verify(ediToolbar, never()).updateUrl("https://www.mozilla.org")
        verify(ediToolbar).updateUrl("Mozilla Firefox")
    }

    @Test
    fun `urlBoxBackgroundDrawable, browserActionMargin and urlBoxMargin are forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = toolbar.displayToolbar

        assertNull(displayToolbar.urlBoxView)
        assertEquals(displayToolbar.browserActionMargin, 0)
        assertEquals(displayToolbar.urlBoxMargin, 0)

        val view = mock(View::class.java)
        toolbar.urlBoxView = view
        toolbar.browserActionMargin = 42
        toolbar.urlBoxMargin = 23

        assertEquals(view, displayToolbar.urlBoxView)
        assertEquals(42, displayToolbar.browserActionMargin)
        assertEquals(23, displayToolbar.urlBoxMargin)
    }

    @Test
    fun `onUrlClicked is forwarded to display toolbar`() {
        val toolbar = BrowserToolbar(context)
        val displayToolbar = toolbar.displayToolbar

        assertTrue(displayToolbar.onUrlClicked())

        toolbar.onUrlClicked = { false }

        assertFalse(displayToolbar.onUrlClicked())
    }

    @Test
    fun `layout of children will factor in padding`() {
        val toolbar = BrowserToolbar(context)
        toolbar.setPadding(50, 20, 60, 15)
        toolbar.removeAllViews()

        val displayToolbar = spy(DisplayToolbar(context, toolbar)).also {
            toolbar.displayToolbar = it
        }

        val editToolbar = spy(EditToolbar(context, toolbar)).also {
            toolbar.editToolbar = it
        }

        listOf(displayToolbar, editToolbar).forEach {
            toolbar.addView(it)
        }

        val widthSpec = View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY)
        val heightSpec = View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY)
        toolbar.measure(widthSpec, heightSpec)

        assertEquals(1000, toolbar.measuredWidth)
        assertEquals(200, toolbar.measuredHeight)

        toolbar.layout(0, 0, 1000, 1000)

        listOf(displayToolbar, editToolbar).forEach {
            assertEquals(890, it.measuredWidth)
            assertEquals(165, it.measuredHeight)

            verify(it).layout(50, 20, 940, 185)
        }
    }

    @Test
    fun `editListener is set on EditToolbar`() {
        val toolbar = BrowserToolbar(context)
        assertNull(toolbar.editToolbar.editListener)

        val listener: Toolbar.OnEditListener = mock()
        toolbar.setOnEditListener(listener)

        assertEquals(listener, toolbar.editToolbar.editListener)
    }

    @Test
    fun `editListener is invoked when switching between modes`() {
        val toolbar = BrowserToolbar(context)

        val listener: Toolbar.OnEditListener = mock()
        toolbar.setOnEditListener(listener)

        toolbar.editMode()

        verify(listener).onStartEditing()
        verifyNoMoreInteractions(listener)

        toolbar.displayMode()

        verify(listener).onStopEditing()
        verifyNoMoreInteractions(listener)
    }

    @Test
    fun `editListener is invoked when text changes`() {
        val toolbar = BrowserToolbar(context)

        val listener: Toolbar.OnEditListener = mock()
        toolbar.setOnEditListener(listener)

        toolbar.editToolbar.urlView.onAttachedToWindow()

        toolbar.editMode()

        toolbar.editToolbar.urlView.setText("Hello")
        toolbar.editToolbar.urlView.setText("Hello World")

        verify(listener).onStartEditing()
        verify(listener).onTextChanged("Hello")
        verify(listener).onTextChanged("Hello World")
    }

    @Test
    fun `BrowserToolbar Button must set padding`() {
        var button = BrowserToolbar.Button(mock(), "imageResource", visible = { true }) {}
        val linearLayout = LinearLayout(context)
        var view = button.createView(linearLayout)
        val padding = Padding(0, 0, 0, 0)
        assertEquals(view.paddingLeft, ACTION_PADDING_DP)
        assertEquals(view.paddingTop, ACTION_PADDING_DP)
        assertEquals(view.paddingRight, ACTION_PADDING_DP)
        assertEquals(view.paddingBottom, ACTION_PADDING_DP)
        button = BrowserToolbar.Button(mock(), "imageResource", padding = padding.copy(left = 16)) {}
        view = button.createView(linearLayout)
        assertEquals(view.paddingLeft, 16)
        button = BrowserToolbar.Button(mock(), "imageResource", padding = padding.copy(top = 16)) {}
        view = button.createView(linearLayout)
        assertEquals(view.paddingTop, 16)
        button = BrowserToolbar.Button(mock(), "imageResource", padding = padding.copy(right = 16)) {}
        view = button.createView(linearLayout)
        assertEquals(view.paddingRight, 16)
        button = BrowserToolbar.Button(mock(), "imageResource", padding = padding.copy(bottom = 16)) {}
        view = button.createView(linearLayout)
        assertEquals(view.paddingBottom, 16)
        button = BrowserToolbar.Button(
            mock(), "imageResource",
            padding = Padding(16, 20, 24, 28)
        ) {}
        view = button.createView(linearLayout)
        view.paddingLeft
        assertEquals(view.paddingLeft, 16)
        assertEquals(view.paddingTop, 20)
        assertEquals(view.paddingRight, 24)
        assertEquals(view.paddingBottom, 28)
    }

    @Test
    fun `BrowserToolbar ToggleButton must set padding`() {
        var button = BrowserToolbar.ToggleButton(
            mock(),
            mock(),
            "imageResource",
            "",
            visible = { true },
            selected = false,
            background = 0
        ) {}
        val linearLayout = LinearLayout(context)
        var view = button.createView(linearLayout)
        assertEquals(view.paddingLeft, ACTION_PADDING_DP)
        assertEquals(view.paddingTop, ACTION_PADDING_DP)
        assertEquals(view.paddingRight, ACTION_PADDING_DP)
        assertEquals(view.paddingBottom, ACTION_PADDING_DP)

        button = BrowserToolbar.ToggleButton(
            mock(),
            mock(),
            "imageResource",
            "",
            visible = { true },
            selected = false,
            background = 0,
            padding = Padding(16, 20, 24, 28)
        ) {}
        view = button.createView(linearLayout)
        view.paddingLeft
        assertEquals(view.paddingLeft, 16)
        assertEquals(view.paddingTop, 20)
        assertEquals(view.paddingRight, 24)
        assertEquals(view.paddingBottom, 28)

        button = BrowserToolbar.ToggleButton(
            mock(),
            mock(),
            "imageResource",
            "",
            selected = false,
            background = 0
        ) {}

        assertTrue(button.visible())
    }

    @Test
    fun `hint changes edit and display urlView`() {
        val toolbar = BrowserToolbar(context)

        assertNull(toolbar.displayToolbar.urlView.hint)
        assertNull(toolbar.editToolbar.urlView.hint)

        toolbar.hint = "hint"

        assertEquals("hint", toolbar.displayToolbar.urlView.hint)
        assertEquals("hint", toolbar.editToolbar.urlView.hint)

        assertEquals(toolbar.displayToolbar.urlView.hint.toString(), toolbar.hint)
    }

    @Test
    fun `hintColor changes edit and display urlView`() {
        val toolbar = BrowserToolbar(context)

        assertTrue(toolbar.displayToolbar.urlView.currentHintTextColor != Color.RED)
        assertTrue(toolbar.editToolbar.urlView.currentHintTextColor != Color.RED)

        toolbar.hintColor = Color.RED

        assertEquals(Color.RED, toolbar.displayToolbar.urlView.currentHintTextColor)
        assertEquals(Color.RED, toolbar.editToolbar.urlView.currentHintTextColor)
    }

    @Test
    fun `textColor changes edit and display urlView`() {
        val toolbar = BrowserToolbar(context)

        assertTrue(toolbar.displayToolbar.urlView.currentTextColor != Color.RED)
        assertTrue(toolbar.editToolbar.urlView.currentTextColor != Color.RED)

        toolbar.textColor = Color.RED

        assertEquals(Color.RED, toolbar.displayToolbar.urlView.currentTextColor)
        assertEquals(Color.RED, toolbar.editToolbar.urlView.currentTextColor)
    }

    @Test
    fun `textSize changes edit and display urlView`() {
        val toolbar = BrowserToolbar(context)

        assertTrue(toolbar.displayToolbar.urlView.textSize != 12f)
        assertTrue(toolbar.editToolbar.urlView.textSize != 12f)

        toolbar.textSize = 12f

        assertEquals(12f, toolbar.displayToolbar.urlView.textSize)
        assertEquals(12f, toolbar.editToolbar.urlView.textSize)
    }

    @Test
    fun `typeface changes edit and display urlView`() {
        val toolbar = BrowserToolbar(context)
        val typeface: Typeface = mock()

        assertNotEquals(typeface, toolbar.editToolbar.urlView.typeface)
        assertNotEquals(typeface, toolbar.displayToolbar.urlView.typeface)

        toolbar.typeface = typeface

        assertEquals(typeface, toolbar.displayToolbar.urlView.typeface)
        assertEquals(typeface, toolbar.editToolbar.urlView.typeface)

        assertEquals(toolbar.displayToolbar.urlView.typeface, toolbar.typeface)
    }

    @Test
    fun `obtainAttributes called when attributes provided`() {
        val application = spy(context)
        val attributeSet: AttributeSet = mock()

        BrowserToolbar(application)
        verify(application, never()).obtainStyledAttributes(attributeSet, R.styleable.BrowserToolbar, 0, 0)

        BrowserToolbar(application, attributeSet)
        verify(application).obtainStyledAttributes(attributeSet, R.styleable.BrowserToolbar, 0, 0)
    }

    @Test
    fun `displaySiteSecurityIcon getter and setter`() {
        val toolbar = BrowserToolbar(context)
        assertEquals(toolbar.displayToolbar.siteSecurityIconView.isVisible(), toolbar.displaySiteSecurityIcon)

        toolbar.displaySiteSecurityIcon = false
        assertEquals(View.GONE, toolbar.displayToolbar.siteSecurityIconView.visibility)

        toolbar.displaySiteSecurityIcon = true
        assertEquals(View.VISIBLE, toolbar.displayToolbar.siteSecurityIconView.visibility)
    }

    @Test
    fun `urlBoxView getter`() {
        val toolbar = BrowserToolbar(context)
        assertEquals(toolbar.displayToolbar.urlBoxView, toolbar.urlBoxView)
    }

    @Test
    fun `browserActionMargin getter`() {
        val toolbar = BrowserToolbar(context)
        assertEquals(toolbar.displayToolbar.browserActionMargin, toolbar.browserActionMargin)
    }

    @Test
    fun `urlBoxMargin getter`() {
        val toolbar = BrowserToolbar(context)
        assertEquals(toolbar.displayToolbar.urlBoxMargin, toolbar.urlBoxMargin)
    }

    @Test
    fun `onUrlClicked getter`() {
        val toolbar = BrowserToolbar(context)
        assertEquals(toolbar.displayToolbar.onUrlClicked, toolbar.onUrlClicked)
    }

    @Test
    fun `setUrlTextPadding applies padding to urlView`() {
        val toolbar = BrowserToolbar(context)
        toolbar.setUrlTextPadding(5, 5, 5, 5)
        assertEquals(5, toolbar.displayToolbar.urlView.paddingLeft)
        assertEquals(5, toolbar.displayToolbar.urlView.paddingTop)
        assertEquals(5, toolbar.displayToolbar.urlView.paddingRight)
        assertEquals(5, toolbar.displayToolbar.urlView.paddingBottom)
    }

    @Test
    fun `Button constructor with drawable`() {
        val buttonDefault = BrowserToolbar.Button(mock(), "imageDrawable") {}

        assertEquals(true, buttonDefault.visible())
        assertEquals(BrowserToolbar.DEFAULT_PADDING, buttonDefault.padding)
        assertEquals("imageDrawable", buttonDefault.contentDescription)

        val button = BrowserToolbar.Button(mock(), "imageDrawable", visible = { false }) {}

        assertEquals(false, button.visible())
    }

    @Test
    fun `ToggleButton constructor with drawable`() {
        val buttonDefault =
            BrowserToolbar.ToggleButton(mock(), mock(), "imageDrawable", "imageSelectedDrawable") {}

        assertEquals(true, buttonDefault.visible())
        assertEquals(BrowserToolbar.DEFAULT_PADDING, buttonDefault.padding)

        val button = BrowserToolbar.ToggleButton(
            mock(),
            mock(),
            "imageDrawable",
            "imageSelectedDrawable",
            visible = { false }) {}

        assertEquals(false, button.visible())
    }

    @Test
    fun `ReloadPageAction visibility changes update image`() {
        val reloadImage: Drawable = mock()
        val stopImage: Drawable = mock()
        val view: ImageButton = mock()
        var reloadPageAction = BrowserToolbar.TwoStateButton(reloadImage, "reload", stopImage, "stop") {}
        assertFalse(reloadPageAction.enabled)
        reloadPageAction.bind(view)
        verify(view).setImageDrawable(stopImage)
        verify(view).contentDescription = "stop"

        reloadPageAction = BrowserToolbar.TwoStateButton(reloadImage, "reload", stopImage, "stop", { false }) {}
        reloadPageAction.bind(view)
        verify(view).setImageDrawable(stopImage)
        verify(view).contentDescription = "reload"
    }

    @Test
    fun `siteSecure updates the displayToolbar`() {
        val toolbar = BrowserToolbar(context)
        toolbar.displayToolbar = spy(toolbar.displayToolbar)
        assertEquals(SiteSecurity.INSECURE, toolbar.siteSecure)

        toolbar.siteSecure = SiteSecurity.SECURE

        verify(toolbar.displayToolbar).setSiteSecurity(SiteSecurity.SECURE)
    }

    @Test
    fun `private flag sets IME_FLAG_NO_PERSONALIZED_LEARNING on url edit view`() {
        val toolbar = BrowserToolbar(context)
        val editToolbar = toolbar.editToolbar

        // By default "private mode" is off.
        assertEquals(0, editToolbar.urlView.imeOptions and
            EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING)
        assertEquals(false, toolbar.private)

        // Turning on private mode sets flag
        toolbar.private = true
        assertNotEquals(0, editToolbar.urlView.imeOptions and
            EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING)
        assertTrue(toolbar.private)

        // Turning private mode off again - should remove flag
        toolbar.private = false
        assertEquals(0, editToolbar.urlView.imeOptions and
            EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING)
        assertEquals(false, toolbar.private)
    }
}
