package org.mozilla.fenix.ui

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performScrollToIndex
import androidx.compose.ui.unit.dp
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.compose.ext.onShown

private const val ON_SHOWN_ROOT_TAG = "onShownRoot"
private const val ON_SHOWN_SETTLE_TIME_MS = 1000
private const val ON_SHOWN_INDEX = 15
private const val ON_SHOWN_NODE_COUNT = 30

class ModifierTest {

    @get:Rule
    val composeTestRule = createComposeRule()

    @Test
    fun verifyModifierOnShownWhenScrolledToWithNoSettleTime() {
        var onShown = false
        composeTestRule.setContent {
            ModifierOnShownContent(
                settleTime = 0,
                onVisible = {
                    onShown = true
                },
            )
        }

        composeTestRule.scrollToOnShownIndex()

        assertTrue(onShown)
    }

    @Test
    fun verifyModifierOnShownAfterSettled() {
        var onShown = false
        composeTestRule.setContent {
            ModifierOnShownContent(
                onVisible = {
                    onShown = true
                },
            )
        }

        composeTestRule.scrollToOnShownIndex()

        assertFalse(onShown)

        composeTestRule.waitUntil(ON_SHOWN_SETTLE_TIME_MS + 500L) { onShown }

        assertTrue(onShown)
    }

    @Test
    fun verifyModifierOnShownWhenNotVisible() {
        val indexToValidate = ON_SHOWN_NODE_COUNT - 1
        var onShown = false
        composeTestRule.setContent {
            ModifierOnShownContent(
                indexToValidate = indexToValidate,
                settleTime = 0,
                onVisible = {
                    onShown = true
                },
            )
        }

        assertFalse(onShown)
    }

    private fun ComposeTestRule.scrollToOnShownIndex(index: Int = ON_SHOWN_INDEX) {
        this.onNodeWithTag(ON_SHOWN_ROOT_TAG)
            .performScrollToIndex(index)
    }

    @Composable
    private fun ModifierOnShownContent(
        indexToValidate: Int = ON_SHOWN_INDEX,
        settleTime: Int = ON_SHOWN_SETTLE_TIME_MS,
        onVisible: () -> Unit,
    ) {
        LazyColumn(
            modifier = Modifier.testTag(ON_SHOWN_ROOT_TAG),
        ) {
            items(ON_SHOWN_NODE_COUNT) { index ->
                val modifier = if (index == indexToValidate) {
                    Modifier.onShown(
                        threshold = 1.0f,
                        settleTime = settleTime,
                        onVisible = onVisible,
                    )
                } else {
                    Modifier
                }

                Text(
                    text = "Test item $index",
                    modifier = modifier
                        .fillMaxWidth()
                        .height(50.dp),
                )
            }
        }
    }
}
