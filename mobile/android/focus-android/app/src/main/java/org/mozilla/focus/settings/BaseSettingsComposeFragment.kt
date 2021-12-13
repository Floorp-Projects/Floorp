/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import androidx.compose.foundation.layout.Column
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.stringResource
import androidx.fragment.app.Fragment
import com.google.accompanist.insets.LocalWindowInsets
import com.google.accompanist.insets.ProvideWindowInsets
import com.google.accompanist.insets.rememberInsetsPaddingValues
import com.google.accompanist.insets.ui.TopAppBar
import org.mozilla.focus.R
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors

open class BaseSettingsComposeFragment : Fragment() {

    /**
     * Displays a toolbar for the screen
     * @param title of the screen.
     * @param onClickUpButton callback for the back arrow of the toolbar
     * @param content of the screen in compose. That will be shown below Toolbar
     */
    @Composable
    fun toolbar(
        title: String,
        onClickUpButton: () -> Unit,
        content: @Composable () -> Unit
    ): ComposeView {
        return ComposeView(requireContext()).apply {
            FocusTheme {
                Scaffold {
                    Column {
                        ProvideWindowInsets {
                            TopAppBar(
                                title = {
                                    Text(
                                        text = title,
                                        color = focusColors.toolbarColor
                                    )
                                },
                                contentPadding = rememberInsetsPaddingValues(
                                    insets = LocalWindowInsets.current.statusBars,
                                    applyStart = true,
                                    applyTop = true,
                                    applyEnd = true,
                                ),
                                navigationIcon = {
                                    IconButton(
                                        onClick = onClickUpButton
                                    ) {
                                        Icon(
                                            Icons.Filled.ArrowBack,
                                            stringResource(R.string.go_back),
                                            tint = focusColors.toolbarColor
                                        )
                                    }
                                },
                                backgroundColor = colorResource(R.color.settings_background)
                            )
                        }
                        content()
                    }
                }
            }
        }
    }
}
