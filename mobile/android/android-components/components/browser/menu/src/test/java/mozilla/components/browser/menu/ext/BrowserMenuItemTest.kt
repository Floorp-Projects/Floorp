package mozilla.components.browser.menu.ext

import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BrowserMenuItemTest {

    @Test
    fun `highest prio item gets selected`() {
        val highlightLow1 = BrowserMenuHighlight.LowPriority(Color.YELLOW)
        val highlightLow2 = BrowserMenuHighlight.LowPriority(Color.RED)
        val highlightHigh = BrowserMenuHighlight.HighPriority(Color.GREEN)

        val list = listOf(
            BrowserMenuHighlightableItem(
                label = "Test1",
                startImageResource = 0,
                highlight = highlightLow1,
                isHighlighted = { true },
            ),
            BrowserMenuHighlightableItem(
                label = "Test2",
                startImageResource = 0,
                highlight = highlightLow2,
                isHighlighted = { true },
            ),
            BrowserMenuImageText(
                label = "Test3",
                imageResource = 0,
            ),
            BrowserMenuHighlightableItem(
                label = "Test4",
                startImageResource = 0,
                highlight = highlightHigh,
                isHighlighted = { true },
            ),
        )
        Assert.assertEquals(highlightHigh, list.getHighlight())
    }

    @Test
    fun `invisible item does not get selected`() {
        val highlightedVisible = BrowserMenuHighlightableItem(
            label = "Test1",
            startImageResource = 0,
            highlight = BrowserMenuHighlight.LowPriority(Color.YELLOW),
            isHighlighted = { true },
        )
        val highlightedInvisible = BrowserMenuHighlightableItem(
            label = "Test2",
            startImageResource = 0,
            highlight = BrowserMenuHighlight.HighPriority(Color.GREEN),
            isHighlighted = { true },
        )
        highlightedInvisible.visible = { false }

        val list = listOf(highlightedVisible, highlightedInvisible)
        Assert.assertEquals(highlightedVisible.highlight, list.getHighlight())
    }

    @Test
    fun `non highlightable item does not get selected`() {
        val highlight = BrowserMenuHighlight.LowPriority(Color.YELLOW)
        val highlight2 = BrowserMenuHighlight.HighPriority(Color.GREEN)
        val list = listOf(
            BrowserMenuHighlightableItem(
                label = "Test1",
                startImageResource = 0,
                highlight = highlight,
                isHighlighted = { true },
            ),
            BrowserMenuHighlightableItem(
                label = "Test2",
                startImageResource = 0,
                highlight = highlight2,
                isHighlighted = { false },
            ),
        )
        Assert.assertEquals(highlight, list.getHighlight())
    }

    @Test
    fun `higher prio highlight which cannot propagate does not get selected`() {
        val highlight = BrowserMenuHighlight.LowPriority(Color.YELLOW)
        val highlightNonPropagate = BrowserMenuHighlight.HighPriority(
            Color.GREEN,
            canPropagate = false,
        )
        val list = listOf(
            BrowserMenuHighlightableItem(
                label = "Test1",
                startImageResource = 0,
                highlight = highlight,
                isHighlighted = { true },
            ),
            BrowserMenuHighlightableItem(
                label = "Test2",
                startImageResource = 0,
                highlight = highlightNonPropagate,
                isHighlighted = { true },
            ),
        )
        Assert.assertEquals(highlight, list.getHighlight())
    }
}
