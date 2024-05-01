/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.cfr

import android.view.View
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView

/**
 * A layout for displaying a [CFRPopup] anchored by [anchorContent].
 *
 * @param showCFR Whether to display the CFR.
 * @param properties [CFRPopupProperties] allowing to customize the popup appearance and behavior.
 * @param onCFRShown Invoked when the CFR is displayed.
 * @param onDismiss Invoked when the CFR is dismissed. Returns true if the dismissal was
 * explicit (e.g. clicked via the "X" button).
 * @param text [Text] block containing the CFR's message.
 * @param action Optional Composable displayed below [text].
 * @param anchorContent The Composable to anchor the CFR to.
 */
@Composable
fun CFRPopupLayout(
    showCFR: Boolean,
    properties: CFRPopupProperties,
    onCFRShown: () -> Unit,
    onDismiss: (Boolean) -> Unit,
    text: @Composable () -> Unit,
    action: @Composable () -> Unit = {},
    anchorContent: @Composable () -> Unit,
) {
    Box(
        modifier = Modifier.height(intrinsicSize = IntrinsicSize.Min),
    ) {
        if (showCFR) {
            LaunchedEffect(Unit) {
                onCFRShown()
            }

            var popup: CFRPopup? = null
            AndroidView(
                modifier = Modifier.fillMaxSize(),
                factory = { context ->
                    View(context).also {
                        popup = CFRPopup(
                            anchor = it,
                            properties = properties,
                            onDismiss = onDismiss,
                            text = text,
                            action = action,
                        )
                    }
                },
                onRelease = {
                    popup?.dismiss()
                    popup = null
                },
                update = {
                    popup?.dismiss()
                    popup?.show()
                },
            )
        }

        anchorContent()
    }
}

/**
 * This is to validate the sizing of the underlying AndroidView. The current implementation of CFRs
 * via [CFRPopupFullscreenLayout] do not render in previews.
 */
@Preview
@Composable
private fun CFRPopupLayoutPreview() {
    var cfrVisible by remember { mutableStateOf(true) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(color = Color.LightGray),
    ) {
        CFRPopupLayout(
            showCFR = cfrVisible,
            properties = CFRPopupProperties(),
            onCFRShown = {},
            onDismiss = { cfrVisible = false },
            text = {
                Text(text = "This is a CFR in Compose")
            },
            action = {
                TextButton(onClick = { cfrVisible = false }) {
                    Text(text = "Dismiss")
                }
            },
        ) {
            Box(modifier = Modifier.size(300.dp)) {
                Text(
                    text = "This is the thing the CFR is anchored to",
                    modifier = Modifier.align(alignment = Alignment.Center),
                )
            }
        }

        Spacer(modifier = Modifier.height(60.dp))

        Box(modifier = Modifier.size(100.dp)) {
            Text(
                text = "This is just another element",
                modifier = Modifier.align(alignment = Alignment.Center),
            )
        }

        Spacer(modifier = Modifier.height(60.dp))

        Button(onClick = { cfrVisible = true }) {
            Text(text = "Show CFR")
        }
    }
}
