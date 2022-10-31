/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.widget

import androidx.core.view.MenuProvider
import java.util.Locale

class BaseVoiceSearchActivityExtendedForTests : BaseVoiceSearchActivity() {

    override fun getCurrentLocale(): Locale {
        return Locale.getDefault()
    }

    override fun onSpeechRecognitionStarted() {
    }

    override fun onSpeechRecognitionEnded(spokenText: String) {
    }

    override fun addMenuProvider(provider: MenuProvider) {
    }
}
