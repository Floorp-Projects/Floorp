/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.utils

import android.view.ViewTreeObserver
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.State
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalView
import org.mozilla.fenix.ext.isKeyboardVisible
import java.lang.ref.WeakReference

/**
 * Detects if the keyboard is opened or closed and returns as a [KeyboardState].
 */
@Composable
fun keyboardAsState(): State<KeyboardState> {
    val keyboardState = remember { mutableStateOf(KeyboardState.Closed) }
    val view = WeakReference(LocalView.current)
    DisposableEffect(view) {
        val onGlobalListener = ViewTreeObserver.OnGlobalLayoutListener {
            keyboardState.value = if (view.get()?.isKeyboardVisible() == true) {
                KeyboardState.Opened
            } else {
                KeyboardState.Closed
            }
        }
        view.get()?.viewTreeObserver?.addOnGlobalLayoutListener(onGlobalListener)

        onDispose {
            view.get()?.viewTreeObserver?.removeOnGlobalLayoutListener(onGlobalListener)
        }
    }

    return keyboardState
}

/**
 * Represents the current state of the keyboard, opened or closed.
 */
enum class KeyboardState {
    Opened, Closed
}
