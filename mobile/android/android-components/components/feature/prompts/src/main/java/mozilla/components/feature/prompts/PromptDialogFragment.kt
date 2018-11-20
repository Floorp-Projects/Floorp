/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.support.annotation.VisibleForTesting
import android.support.v4.app.DialogFragment

internal const val KEY_SESSION_ID = "KEY_SESSION_ID"

/**
 * An abstract representation for all different types of prompt dialogs.
 * for handling [PromptFeature] dialogs.
 */
abstract class PromptDialogFragment : DialogFragment() {

    var feature: PromptFeature? = null

    @VisibleForTesting
    internal val sessionId: String by lazy { requireNotNull(arguments).getString(KEY_SESSION_ID)!! }
}
