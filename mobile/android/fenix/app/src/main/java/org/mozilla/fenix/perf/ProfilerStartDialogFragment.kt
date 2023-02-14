/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.fragment.app.activityViewModels
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.concept.base.profiler.Profiler
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

/**
 * Dialogue to start the Gecko profiler in Fenix without the use of ADB.
 */
class ProfilerStartDialogFragment : AppCompatDialogFragment() {

    private lateinit var viewScope: CoroutineScope

    private val delayToPollProfilerForStatus = 100L
    private lateinit var profiler: Profiler
    private val profilerViewModel: ProfilerViewModel by activityViewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        viewScope = MainScope()

        profiler = requireContext().components.core.engine.profiler!!
        return ComposeView(requireContext()).apply {
            setContent {
                StartProfileDialog(context.components.core.engine.profiler!!::startProfiler)
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        viewScope.cancel()
    }

    override fun dismiss() {
        profilerViewModel.setProfilerState(requireContext().components.core.engine.profiler!!.isProfilerActive())
        super.dismiss()
    }

    @Composable
    private fun StartProfileDialog(
        startProfiler: (Array<String>, Array<String>) -> Unit,
    ) {
        val viewStateObserver = remember { mutableStateOf(CardState.ChooseSettings) }

        Dialog(
            onDismissRequest = {
                // In the wait for profiler state, the user needs to wait for the profiler to start
                // so it'd be counterproductive to allow them dismiss the dialog.
                if (viewStateObserver.value != CardState.WaitForProfilerToStart) {
                    this@ProfilerStartDialogFragment.dismiss()
                }
            },
        ) {
            if (viewStateObserver.value == CardState.ChooseSettings) {
                StartCard(viewStateObserver, startProfiler)
            } else {
                WaitForProfilerDialog(R.string.profiler_waiting_start)
            }
        }
    }

    @SuppressWarnings("LongMethod")
    @Composable
    private fun StartCard(
        viewStateObserver: MutableState<CardState>,
        startProfiler: (Array<String>, Array<String>) -> Unit,
    ) {
        val featureAndThreadsObserver = remember {
            mutableStateOf(requireContext().resources.getString(R.string.profiler_filter_firefox))
        }
        ProfilerDialogueCard {
            Column(modifier = Modifier.padding(8.dp)) {
                Text(
                    text = stringResource(R.string.preferences_start_profiler),
                    fontWeight = FontWeight.ExtraBold,
                    fontSize = 20.sp,
                    modifier = Modifier.padding(8.dp),
                )
                Text(
                    text = stringResource(R.string.profiler_settings_title),
                    fontWeight = FontWeight.Bold,
                    fontSize = 15.sp,
                    modifier = Modifier.padding(8.dp),
                )
                Spacer(modifier = Modifier.height(2.dp))
                ProfilerLabeledRadioButton(
                    text = stringResource(R.string.profiler_filter_firefox),
                    subText = stringResource(R.string.profiler_filter_firefox_explain),
                    selected = featureAndThreadsObserver.value == stringResource(R.string.profiler_filter_firefox),
                    onClick = {
                        featureAndThreadsObserver.value = getString(R.string.profiler_filter_firefox)
                    },
                )

                ProfilerLabeledRadioButton(
                    text = stringResource(R.string.profiler_filter_graphics),
                    subText = stringResource(R.string.profiler_filter_graphics_explain),
                    selected = featureAndThreadsObserver.value == stringResource(R.string.profiler_filter_graphics),
                    onClick = {
                        featureAndThreadsObserver.value = getString(R.string.profiler_filter_graphics)
                    },
                )

                ProfilerLabeledRadioButton(
                    text = stringResource(R.string.profiler_filter_media),
                    subText = stringResource(R.string.profiler_filter_media_explain),
                    selected = featureAndThreadsObserver.value == stringResource(R.string.profiler_filter_media),
                    onClick = {
                        featureAndThreadsObserver.value = getString(R.string.profiler_filter_media)
                    },
                )

                ProfilerLabeledRadioButton(
                    text = stringResource(R.string.profiler_filter_networking),
                    subText = stringResource(R.string.profiler_filter_networking_explain),
                    selected = featureAndThreadsObserver.value == stringResource(R.string.profiler_filter_networking),
                    onClick = {
                        featureAndThreadsObserver.value = getString(R.string.profiler_filter_networking)
                    },
                )
                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    horizontalArrangement = Arrangement.End,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    TextButton(
                        onClick = {
                            this@ProfilerStartDialogFragment.dismiss()
                        },
                    ) {
                        Text(text = stringResource(R.string.profiler_start_cancel))
                    }
                    Spacer(modifier = Modifier.width(4.dp))
                    TextButton(
                        onClick = {
                            viewStateObserver.value = CardState.WaitForProfilerToStart
                            executeStartProfilerOnClick(
                                ProfilerSettings.valueOf(featureAndThreadsObserver.value),
                                startProfiler,
                            )
                        },
                    ) {
                        Text(text = stringResource(R.string.preferences_start_profiler))
                    }
                }
            }
        }
    }

    private fun waitForProfilerActiveAndDismissFragment() {
        viewScope.launch {
            while (!profiler.isProfilerActive()) {
                delay(delayToPollProfilerForStatus)
            }
            this@ProfilerStartDialogFragment.dismiss()

            val toastString = requireContext().getString(R.string.profiler_start_dialog_started)
            Toast.makeText(this@ProfilerStartDialogFragment.context, toastString, Toast.LENGTH_SHORT).show()
        }
    }

    private fun executeStartProfilerOnClick(
        featureAndThreads: ProfilerSettings,
        startProfiler: (Array<String>, Array<String>) -> Unit,
    ) {
        startProfiler(featureAndThreads.threads, featureAndThreads.features)
        waitForProfilerActiveAndDismissFragment()
    }

    /**
     * Card state to change what is displayed in the dialogue
     */
    enum class CardState {
        ChooseSettings,
        WaitForProfilerToStart,
    }
}
