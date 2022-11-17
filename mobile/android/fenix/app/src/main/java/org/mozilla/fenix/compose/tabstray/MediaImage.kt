/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.tabstray

import android.content.res.Configuration
import androidx.appcompat.content.res.AppCompatResources
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.google.accompanist.drawablepainter.rememberDrawablePainter
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.mediasession.MediaSession.PlaybackState
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Controller buttons for the media (play/pause) state for the given [tab].
 *
 * @param tab [TabSessionState] which the image should be shown.
 * @param onMediaIconClicked handles the click event when tab has media session like play/pause.
 * @param modifier [Modifier] to be applied to the layout.
 */
@Composable
fun MediaImage(
    tab: TabSessionState,
    onMediaIconClicked: ((TabSessionState) -> Unit),
    modifier: Modifier,
) {
    val (icon, contentDescription) = when (tab.mediaSessionState?.playbackState) {
        PlaybackState.PAUSED -> {
            R.drawable.media_state_play to R.string.mozac_feature_media_notification_action_play
        }
        PlaybackState.PLAYING -> {
            R.drawable.media_state_pause to R.string.mozac_feature_media_notification_action_pause
        }
        else -> return
    }
    val drawable = AppCompatResources.getDrawable(LocalContext.current, icon)
    // Follow up ticket https://github.com/mozilla-mobile/fenix/issues/25774
    Image(
        painter = rememberDrawablePainter(drawable = drawable),
        contentDescription = stringResource(contentDescription),
        modifier = modifier.clickable { onMediaIconClicked(tab) },
    )
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun ImagePreview() {
    FirefoxTheme {
        MediaImage(
            tab = createTab(url = "https://mozilla.com"),
            onMediaIconClicked = {},
            modifier = Modifier
                .height(100.dp)
                .width(200.dp),
        )
    }
}
