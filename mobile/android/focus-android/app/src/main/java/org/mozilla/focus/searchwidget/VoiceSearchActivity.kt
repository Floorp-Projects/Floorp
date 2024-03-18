/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchwidget

import android.content.Intent
import mozilla.components.feature.search.widget.BaseVoiceSearchActivity
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.locale.LocaleManager.getCurrentLocale
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.SearchWidget
import org.mozilla.focus.activity.IntentReceiverActivity
import java.util.Locale

class VoiceSearchActivity : BaseVoiceSearchActivity() {

    override fun getCurrentLocale(): Locale {
        return getCurrentLocale(this)
            ?: LocaleManager.getSystemDefault()
    }

    override fun onSpeechRecognitionEnded(spokenText: String) {
        val intent = Intent(this, IntentReceiverActivity::class.java)
        intent.putExtra(SPEECH_PROCESSING, spokenText)
        startActivity(intent)
    }

    override fun onSpeechRecognitionStarted() {
        SearchWidget.voiceButton.record(NoExtras())
    }
}
