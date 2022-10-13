/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.widget

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.Intent
import android.os.Bundle
import android.speech.RecognizerIntent
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatActivity
import mozilla.components.support.base.log.logger.Logger
import java.util.Locale

/**
 * Launches voice recognition then uses it to start a new web search.
 */
abstract class BaseVoiceSearchActivity : AppCompatActivity() {

    /**
     * Holds the intent that initially started this activity
     * so that it can persist through the speech activity.
     */
    private var previousIntent: Intent? = null

    private var activityResultLauncher: ActivityResultLauncher<Intent> = getActivityResultLauncher()

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putParcelable(PREVIOUS_INTENT, previousIntent)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Retrieve the previous intent from the saved state
        previousIntent = savedInstanceState?.get(PREVIOUS_INTENT) as Intent?
        if (previousIntent.isForSpeechProcessing()) {
            // Don't reopen the speech recognizer
            return
        }

        // The intent property is nullable, but the rest of the code below assumes it is not.
        val intent = intent?.let { Intent(intent) } ?: Intent()
        if (intent.isForSpeechProcessing()) {
            previousIntent = intent
            displaySpeechRecognizer()
        } else {
            finish()
        }
    }

    /**
     * Language locale for Voice Search.
     */
    abstract fun getCurrentLocale(): Locale

    /**
     * Speech recognizer popup is shown.
     */
    abstract fun onSpeechRecognitionStarted()

    /**
     * Start intent after voice search ,for example a browser page is open with the spokenText.
     * @param spokenText what the user voice search
     */
    abstract fun onSpeechRecognitionEnded(spokenText: String)

    @VisibleForTesting
    internal fun activityResultImplementation(activityResult: ActivityResult) {
        if (activityResult.resultCode == Activity.RESULT_OK) {
            val spokenText =
                activityResult.data?.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS)
                    ?.first()
            previousIntent?.apply {
                spokenText?.let { onSpeechRecognitionEnded(it) }
            }
        }
        finish()
    }

    private fun getActivityResultLauncher(): ActivityResultLauncher<Intent> {
        return registerForActivityResult(
            ActivityResultContracts.StartActivityForResult(),
        ) {
            activityResultImplementation(it)
        }
    }

    /**
     * Displays a speech recognizer popup that listens for input from the user.
     */
    private fun displaySpeechRecognizer() {
        val intentSpeech = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            putExtra(
                RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                RecognizerIntent.LANGUAGE_MODEL_FREE_FORM,
            )
            putExtra(
                RecognizerIntent.EXTRA_LANGUAGE,
                getCurrentLocale(),
            )
        }
        onSpeechRecognitionStarted()
        try {
            activityResultLauncher.launch(intentSpeech)
        } catch (e: ActivityNotFoundException) {
            Logger(TAG).error("ActivityNotFoundException " + e.message.toString())
            finish()
        }
    }

    /**
     * Returns true if the [SPEECH_PROCESSING] extra is present and set to true.
     * Returns false if the intent is null.
     */
    private fun Intent?.isForSpeechProcessing(): Boolean =
        this?.getBooleanExtra(SPEECH_PROCESSING, false) == true

    companion object {
        const val PREVIOUS_INTENT = "org.mozilla.components.previous_intent"

        /**
         * In [BaseVoiceSearchActivity] activity, used to store if the speech processing should start.
         */
        const val SPEECH_PROCESSING = "speech_processing"
        const val TAG = "BaseVoiceSearchActivity"
    }
}
