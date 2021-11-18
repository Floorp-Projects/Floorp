/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions

import android.content.Context
import org.mozilla.focus.R

sealed class AutoplayOption(open val prefKeyId: Int, open val textId: Int) {
    data class AllowAudioVideo(
        override val prefKeyId: Int = R.string.pref_key_allow_autoplay_audio_video,
        override val textId: Int = R.string.preference_allow_audio_video_autoplay
    ) : AutoplayOption(prefKeyId = prefKeyId, textId = textId)

    data class BlockAudioOnly(
        override val prefKeyId: Int = R.string.pref_key_block_autoplay_audio_only,
        override val textId: Int = R.string.preference_block_autoplay_audio_only,
    ) : AutoplayOption(prefKeyId = prefKeyId, textId = textId)

    data class BlockAudioVideo(
        override val prefKeyId: Int = R.string.pref_key_block_autoplay_audio_video,
        override val textId: Int = R.string.preference_block_autoplay_audio_video
    ) : AutoplayOption(prefKeyId = prefKeyId, textId = textId)
}

fun getValueByPrefKey(autoplayPrefKey: String?, context: Context) =
    when (autoplayPrefKey) {
        context.getString(R.string.pref_key_allow_autoplay_audio_video) -> AutoplayOption.AllowAudioVideo()
        context.getString(R.string.pref_key_block_autoplay_audio_video) -> AutoplayOption.BlockAudioVideo()
        else -> AutoplayOption.BlockAudioOnly()
    }
