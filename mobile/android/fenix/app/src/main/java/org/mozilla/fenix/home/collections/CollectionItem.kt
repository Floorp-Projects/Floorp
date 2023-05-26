/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.DismissDirection.EndToStart
import androidx.compose.material.DismissDirection.StartToEnd
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.SwipeToDismiss
import androidx.compose.material.rememberDismissState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.Tab
import org.mozilla.fenix.R.drawable
import org.mozilla.fenix.R.string
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.FaviconListItem
import org.mozilla.fenix.compose.tabstray.DismissedTabBackground
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.theme.FirefoxTheme
import java.io.File

/**
 * Rectangular shape with only right angles used to display a middle tab.
 */
private val MIDDLE_TAB_SHAPE = RoundedCornerShape(0.dp)

/**
 * Rectangular shape with only the bottom corners rounded used to display the last tab in a collection.
 */
private val BOTTOM_TAB_SHAPE = RoundedCornerShape(bottomStart = 8.dp, bottomEnd = 8.dp)

/**
 * Display an individual [Tab] as part of a collection.
 *
 * @param tab [Tab] to display.
 * @param isLastInCollection Whether the tab is to be shown between others or as the last one in collection.
 * @param onClick Invoked when the user click on the tab.
 * @param onRemove Invoked when the user removes the tab informing also if the tab was swiped to be removed.
 */
@OptIn(ExperimentalMaterialApi::class)
@Composable
fun CollectionItem(
    tab: Tab,
    isLastInCollection: Boolean,
    onClick: () -> Unit,
    onRemove: (Boolean) -> Unit,
) {
    val dismissState = rememberDismissState()

    if (dismissState.isDismissed(StartToEnd) || dismissState.isDismissed(EndToStart)) {
        onRemove(true)
    }

    SwipeToDismiss(
        state = dismissState,
        background = {
            DismissedTabBackground(
                dismissDirection = dismissState.dismissDirection,
                shape = if (isLastInCollection) BOTTOM_TAB_SHAPE else MIDDLE_TAB_SHAPE,
            )
        },
    ) {
        // We need to clip the top bounds to avoid this item drawing shadows over the above item.
        // But we need to add this shadows back to have a clearer separation between tabs
        // when one is being swiped away.
        val clippingModifier by remember {
            derivedStateOf {
                try {
                    if (dismissState.progress.fraction != 1f) Modifier else Modifier.clipTop()
                } catch (e: NoSuchElementException) {
                    // `androidx.compose.material.Swipeable.findBounds` couldn't find anchors.
                    // Happened once in testing when deleting a tab. Could not reproduce afterwards.
                    Modifier.clipTop()
                }
            }
        }

        Card(
            modifier = clippingModifier
                .fillMaxWidth(),
            shape = if (isLastInCollection) BOTTOM_TAB_SHAPE else MIDDLE_TAB_SHAPE,
            backgroundColor = FirefoxTheme.colors.layer2,
            elevation = 5.dp,
        ) {
            FaviconListItem(
                label = tab.title,
                description = tab.url.toShortUrl(),
                onClick = onClick,
                url = tab.url,
                iconPainter = painterResource(drawable.ic_close),
                iconDescription = stringResource(string.remove_tab_from_collection),
                onIconClick = { onRemove(false) },
            )
        }
    }
}

/**
 * Clips the Composable this applies to such that it cannot draw content / shadows outside it's top bound.
 */
private fun Modifier.clipTop() = this.then(
    Modifier.drawWithContent {
        val paddingPx = Constraints.Infinity.toFloat()
        clipRect(
            left = 0f - paddingPx,
            top = 0f,
            right = size.width + paddingPx,
            bottom = size.height + paddingPx,
        ) {
            this@drawWithContent.drawContent()
        }
    },
)

@Composable
@LightDarkPreview
private fun TabInCollectionPreview() {
    FirefoxTheme {
        Column {
            CollectionItem(
                tab = tabPreview,
                isLastInCollection = false,
                onClick = {},
                onRemove = {},
            )

            Spacer(Modifier.height(10.dp))

            CollectionItem(
                tab = tabPreview,
                isLastInCollection = true,
                onClick = {},
                onRemove = {},
            )
        }
    }
}

private val tabPreview = object : Tab {
    override val id = 2L
    override val title = "Mozilla-Firefox"
    override val url = "https://www.mozilla.org/en-US/firefox/whats-new-in-last-version"

    override fun restore(
        filesDir: File,
        engine: Engine,
        restoreSessionId: Boolean,
    ): RecoverableTab? = null
}
