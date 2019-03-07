/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import androidx.fragment.app.DialogFragment

internal const val KEY_SESSION_ID = "KEY_SESSION_ID"
internal const val KEY_TITLE = "KEY_TITLE"
internal const val KEY_MESSAGE = "KEY_MESSAGE"
/**
 * An abstract representation for all different types of prompt dialogs.
 * for handling [PromptFeature] dialogs.
 */
internal abstract class PromptDialogFragment : DialogFragment() {

    var feature: PromptFeature? = null

    internal val sessionId: String by lazy { requireNotNull(arguments).getString(KEY_SESSION_ID)!! }

    internal val title: String by lazy { safeArguments.getString(KEY_TITLE)!! }

    internal val message: String by lazy { safeArguments.getString(KEY_MESSAGE)!! }

    val safeArguments get() = requireNotNull(arguments)
}
