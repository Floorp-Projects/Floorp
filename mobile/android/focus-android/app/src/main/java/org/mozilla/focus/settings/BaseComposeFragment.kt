/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("UnusedMaterialScaffoldPaddingParameter")

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.fragment.app.Fragment
import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.ext.hideToolbar
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors

/**
 * Fragment acting as a wrapper over a [Composable] which will be shown below a [TopAppBar].
 *
 * Useful for Fragments shown in otherwise fullscreen Activities such that they would be shown
 * beneath the status bar not below it.
 *
 * Classes extending this are expected to provide the [Composable] content and a basic behavior
 * for the toolbar: title and navigate up callback.
 */
abstract class BaseComposeFragment : Fragment() {
    /**
     * Screen title shown in toolbar.
     */
    open val titleRes: Int? = null

    open val titleText: String? = null

    /**
     * Callback for the up navigation button shown in toolbar.
     */
    abstract fun onNavigateUp(): () -> Unit

    /**
     * content of the screen in compose. That will be shown below Toolbar
     */
    @Composable
    abstract fun Content()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        hideToolbar()
        (requireActivity() as? MainActivity)?.hideStatusBarBackground()

        return ComposeView(requireContext()).apply {
            setContent {
                val title = getTitle()
                FocusTheme {
                    Scaffold(
                        modifier = Modifier
                            .background(colorResource(id = R.color.settings_background))
                            .statusBarsPadding(),
                    ) { paddingValues ->
                        Column {
                            TopAppBar(
                                title = title,
                                modifier = Modifier.padding(paddingValues),
                                onNavigateUpClick = onNavigateUp(),
                            )
                            this@BaseComposeFragment.Content()
                        }
                    }
                }
            }
            isTransitionGroup = true
        }
    }

    private fun getTitle(): String {
        var title = ""
        titleRes?.let { title = getString(it) }
        titleText?.let { title = it }
        return title
    }
}

@Composable
private fun TopAppBar(
    title: String,
    modifier: Modifier = Modifier,
    onNavigateUpClick: () -> Unit,
) {
    TopAppBar(
        title = {
            Text(
                text = title,
                color = focusColors.toolbarColor,
            )
        },
        modifier = modifier,
        navigationIcon = {
            IconButton(
                onClick = onNavigateUpClick,
            ) {
                Icon(
                    painterResource(id = R.drawable.mozac_ic_back_24),
                    stringResource(R.string.go_back),
                    tint = focusColors.toolbarColor,
                )
            }
        },
        backgroundColor = colorResource(R.color.settings_background),
    )
}
