/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions

import org.mozilla.focus.R

sealed class AutoplayOption {

    data class AllowAudioVideo(
        override val prefKeyId: Int = R.string.pref_key_allow_autoplay_audio_video,
        override val titleId: Int = R.string.preference_allow_audio_video_autoplay,
    ) : SitePermissionOption(prefKeyId = prefKeyId, titleId = titleId)

    data class BlockAudioOnly(
        override val prefKeyId: Int = R.string.pref_key_block_autoplay_audio_only,
        override val titleId: Int = R.string.preference_block_autoplay_audio_only,
        override val summaryId: Int = R.string.preference_block_autoplay_audio_only_summary,
    ) : SitePermissionOption(prefKeyId = prefKeyId, titleId = titleId)

    data class BlockAudioVideo(
        override val prefKeyId: Int = R.string.pref_key_block_autoplay_audio_video,
        override val titleId: Int = R.string.preference_block_autoplay_audio_video,
    ) : SitePermissionOption(prefKeyId = prefKeyId, titleId = titleId)
}

sealed class SitePermissionOption(open val prefKeyId: Int, open val titleId: Int, open val summaryId: Int? = null) {

    data class AskToAllow(
        override val prefKeyId: Int = R.string.pref_key_ask_to_allow,
        override val titleId: Int = R.string.preference_option_phone_feature_ask_to_allow,
        override val summaryId: Int = R.string.preference_block_autoplay_audio_only_summary,
    ) : SitePermissionOption(prefKeyId = prefKeyId, titleId = titleId)

    data class Blocked(
        override val prefKeyId: Int = R.string.pref_key_blocked,
        override val titleId: Int = R.string.preference_option_phone_feature_blocked,
    ) : SitePermissionOption(prefKeyId = prefKeyId, titleId = titleId)

    data class Allowed(
        override val prefKeyId: Int = R.string.pref_key_allowed,
        override val titleId: Int = R.string.preference_option_phone_feature_allowed,
    ) : SitePermissionOption(prefKeyId = prefKeyId, titleId = titleId)
}
