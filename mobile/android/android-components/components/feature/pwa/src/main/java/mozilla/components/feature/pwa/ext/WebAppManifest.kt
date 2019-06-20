/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.app.ActivityManager.TaskDescription
import mozilla.components.concept.engine.manifest.WebAppManifest

/**
 * Create a [TaskDescription] for the activity manager based on the manifest.
 *
 * Since the web app icon is provided dynamically by the web site, we can't provide a resource ID.
 * Instead we use the deprecated constructor.
 */
@Suppress("Deprecation")
fun WebAppManifest.asTaskDescription() =
    TaskDescription(name, null, themeColor ?: 0)
