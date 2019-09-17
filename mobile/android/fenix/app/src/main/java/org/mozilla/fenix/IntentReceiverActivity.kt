/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.speech.RecognizerIntent
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.customtabs.AuthCustomTabActivity
import org.mozilla.fenix.customtabs.AuthCustomTabActivity.Companion.EXTRA_AUTH_CUSTOM_TAB
import org.mozilla.fenix.customtabs.CustomTabActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor

class IntentReceiverActivity : Activity() {

    // Holds the intent that initially started this activity
    // so that it can persist through the speech activity.
    private var previousIntent: Intent? = null

    @Suppress("ComplexMethod")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        previousIntent = savedInstanceState?.get(PREVIOUS_INTENT) as Intent?
        if (previousIntent?.getBooleanExtra(SPEECH_PROCESSING, false) == true) {
            return
        }

        MainScope().launch {
            // The intent property is nullable, but the rest of the code below
            // assumes it is not. If it's null, then we make a new one and open
            // the HomeActivity.
            val intent = intent?.let { Intent(intent) } ?: Intent()

            val intentProcessors = listOf(
                components.utils.customTabIntentProcessor,
                components.utils.intentProcessor
            )

            if (intent.getBooleanExtra(SPEECH_PROCESSING, false)) {
                previousIntent = intent
                displaySpeechRecognizer()
            } else {
                intentProcessors.any { it.process(intent) }
                setIntentActivity(intent)

                startActivity(intent)

                finish()
            }
        }
    }

    private fun setIntentActivity(intent: Intent) {
        val openToBrowser = when {
            components.utils.customTabIntentProcessor.matches(intent) -> {
                // TODO this needs to change: https://github.com/mozilla-mobile/fenix/issues/5225
                val activityClass = if (intent.hasExtra(EXTRA_AUTH_CUSTOM_TAB)) {
                    AuthCustomTabActivity::class
                } else {
                    CustomTabActivity::class
                }
                intent.setClassName(applicationContext, activityClass.java.name)
                true
            }
            intent.action == Intent.ACTION_VIEW || intent.action == Intent.ACTION_SEND -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                // This Intent was launched from history (recent apps). Android will redeliver the
                // original Intent (which might be a VIEW intent). However if there's no active browsing
                // session then we do not want to re-process the Intent and potentially re-open a website
                // from a session that the user already "erased".
                intent.flags and Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY == 0
            }
            intent.action == ACTION_OPEN_TAB || intent.action == ACTION_OPEN_PRIVATE_TAB -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                val startPrivateMode = (intent.action == ACTION_OPEN_PRIVATE_TAB)
                if (startPrivateMode) {
                    intent.putExtra(
                        HomeActivity.OPEN_TO_SEARCH,
                        StartSearchIntentProcessor.STATIC_SHORTCUT_NEW_PRIVATE_TAB
                    )
                } else {
                    intent.putExtra(
                        HomeActivity.OPEN_TO_SEARCH,
                        StartSearchIntentProcessor.STATIC_SHORTCUT_NEW_TAB
                    )
                }
                intent.putExtra(HomeActivity.PRIVATE_BROWSING_MODE, startPrivateMode)
                intent.flags = intent.flags or Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                true
            }
            else -> {
                intent.setClassName(applicationContext, HomeActivity::class.java.name)
                false
            }
        }

        intent.putExtra(HomeActivity.OPEN_TO_BROWSER, openToBrowser)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putParcelable(PREVIOUS_INTENT, previousIntent)
    }

    private fun displaySpeechRecognizer() {
        val intentSpeech = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
        }
        this.metrics.track(Event.SearchWidgetVoiceSearchPressed)
        startActivityForResult(intentSpeech, SPEECH_REQUEST_CODE)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        if (requestCode == SPEECH_REQUEST_CODE && resultCode == RESULT_OK) {
            val spokenText: String? =
                data?.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS)?.let { results ->
                    results[0]
                }

            previousIntent?.let {
                it.putExtra(SPEECH_PROCESSING, spokenText)
                it.putExtra(HomeActivity.OPEN_TO_BROWSER_AND_LOAD, true)
                startActivity(it)
            }
        }

        finish()
    }

    companion object {
        private const val SPEECH_REQUEST_CODE = 0
        const val SPEECH_PROCESSING = "speech_processing"
        const val PREVIOUS_INTENT = "previous_intent"
        const val ACTION_OPEN_TAB = "org.mozilla.fenix.OPEN_TAB"
        const val ACTION_OPEN_PRIVATE_TAB = "org.mozilla.fenix.OPEN_PRIVATE_TAB"
    }
}
