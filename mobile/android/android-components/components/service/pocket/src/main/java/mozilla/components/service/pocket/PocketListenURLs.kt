/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import android.net.Uri
import androidx.core.net.toUri

// When upgrading to production endpoint, ensure this is HTTPS.
private const val STAGE_SERVER_BASE = "http://scout-stage.herokuapp.com"

/**
 * A collection of URLs available through the Pocket Listen API.
 */
internal class PocketListenURLs {

    val articleService: Uri = "$STAGE_SERVER_BASE/command/articleservice".toUri()
}
