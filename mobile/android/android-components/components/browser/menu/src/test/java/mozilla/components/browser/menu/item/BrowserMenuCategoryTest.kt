package mozilla.components.browser.menu.item

import android.content.Context
import android.graphics.Typeface
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserMenuCategoryTest {
    private lateinit var menuCategory: BrowserMenuCategory
    private val context: Context get() = ApplicationProvider.getApplicationContext()
    private val label = "test"

    @Before
    fun setup() {
        menuCategory = BrowserMenuCategory(label)
    }

    @Test
    fun `menu category uses correct layout`() {
        assertEquals(R.layout.mozac_browser_menu_category, menuCategory.getLayoutResource())
    }

    @Test
    fun `menu category  has correct label`() {
        assertEquals(label, menuCategory.label)
    }

    @Test
    fun `menu category should handle initialization with text size`() {
        val menuCategoryWithTextSize = BrowserMenuCategory(label, 12f)

        val view = inflate(menuCategoryWithTextSize)
        val textView = view.findViewById<TextView>(R.id.category_text)

        assertEquals(12f, textView.textSize)
    }

    @Test
    fun `menu category should handle initialization with text colour resource`() {
        val menuCategoryWithTextColour = BrowserMenuCategory(label, textColorResource = android.R.color.holo_red_dark)

        val view = inflate(menuCategoryWithTextColour)
        val textView = view.findViewById<TextView>(R.id.category_text)
        val expectedColour = ContextCompat.getColor(textView.context, android.R.color.holo_red_dark)

        assertEquals(expectedColour, textView.currentTextColor)
    }

    @Test
    fun `GIVEN a BrowserMenuCategory, WHEN backgroundColorResource is provided, THEN the background resource is set to that value`() {
        val expectedColour = android.R.color.holo_red_dark
        val menuCategoryWithBackgroundColour = BrowserMenuCategory(label, backgroundColorResource = expectedColour)
        val view: TextView = mock()
        val menu: BrowserMenu = mock()

        menuCategoryWithBackgroundColour.bind(menu, view)

        verify(view).setBackgroundResource(expectedColour)
    }

    @Test
    fun `GIVEN a BrowserMenuCategory, WHEN backgroundColorResource is not provided, THEN no background is set`() {
        val menuCategoryWithNoBackgroundColour = BrowserMenuCategory(label)
        val view: TextView = mock()
        val menu: BrowserMenu = mock()

        menuCategoryWithNoBackgroundColour.bind(menu, view)

        verify(view, never()).setBackgroundResource(anyInt())
    }

    @Test
    fun `menu category should handle initialization with text style`() {
        val menuCategoryWithTextStyle = BrowserMenuCategory(label, textStyle = Typeface.ITALIC)

        val view = inflate(menuCategoryWithTextStyle)
        val textView = view.findViewById<TextView>(R.id.category_text)

        assertEquals(Typeface.ITALIC, textView.typeface.style)
    }

    @Test
    fun `menu category should handle initialization with text alignment`() {
        val menuCategoryWithTextAlignment = BrowserMenuCategory(label, textAlignment = View.TEXT_ALIGNMENT_VIEW_END)

        val view = inflate(menuCategoryWithTextAlignment)
        val textView = view.findViewById<TextView>(R.id.category_text)

        assertEquals(View.TEXT_ALIGNMENT_VIEW_END, textView.textAlignment)
    }

    @Test
    fun `menu category can be converted to candidate`() {
        assertEquals(
            DecorativeTextMenuCandidate(
                label,
                textStyle = TextStyle(
                    textStyle = Typeface.BOLD,
                    textAlignment = View.TEXT_ALIGNMENT_VIEW_START,
                ),
            ),
            BrowserMenuCategory(label).asCandidate(context),
        )

        assertEquals(
            DecorativeTextMenuCandidate(
                label,
                textStyle = TextStyle(
                    size = 12f,
                    textStyle = Typeface.BOLD,
                    textAlignment = View.TEXT_ALIGNMENT_VIEW_START,
                ),
            ),
            BrowserMenuCategory(label, 12f).asCandidate(context),
        )

        assertEquals(
            DecorativeTextMenuCandidate(
                label,
                textStyle = TextStyle(
                    color = ContextCompat.getColor(context, android.R.color.holo_red_dark),
                    textStyle = Typeface.BOLD,
                    textAlignment = View.TEXT_ALIGNMENT_VIEW_START,
                ),
            ),
            BrowserMenuCategory(
                label,
                textColorResource = android.R.color.holo_red_dark,
            ).asCandidate(context),
        )

        assertEquals(
            DecorativeTextMenuCandidate(
                label,
                textStyle = TextStyle(
                    textStyle = Typeface.ITALIC,
                    textAlignment = View.TEXT_ALIGNMENT_VIEW_START,
                ),
            ),
            BrowserMenuCategory(label, textStyle = Typeface.ITALIC).asCandidate(context),
        )

        assertEquals(
            DecorativeTextMenuCandidate(
                label,
                textStyle = TextStyle(
                    textStyle = Typeface.BOLD,
                    textAlignment = View.TEXT_ALIGNMENT_VIEW_END,
                ),
            ),
            BrowserMenuCategory(label, textAlignment = View.TEXT_ALIGNMENT_VIEW_END).asCandidate(context),
        )
    }

    private fun inflate(browserMenuCategory: BrowserMenuCategory): View {
        val view = LayoutInflater.from(context).inflate(browserMenuCategory.getLayoutResource(), null)
        val mockMenu = Mockito.mock(BrowserMenu::class.java)
        browserMenuCategory.bind(mockMenu, view)
        return view
    }
}
