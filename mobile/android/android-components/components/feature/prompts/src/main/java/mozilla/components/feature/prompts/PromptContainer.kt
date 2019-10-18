/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import android.content.Context
import android.content.Intent
import androidx.annotation.StringRes

/**
 * Wrapper to hold shared functionality between activities and fragments for [PromptFeature].
 */
internal sealed class PromptContainer {

    /**
     * Getter for [Context].
     */
    abstract val context: Context

    /**
     * Launches an activity for which you would like a result when it finished.
     */
    abstract fun startActivityForResult(intent: Intent, code: Int)

    /**
     * Returns a localized string.
     */
    abstract fun getString(@StringRes res: Int): String

    internal class Activity(
        private val activity: android.app.Activity
    ) : PromptContainer() {

        override val context get() = activity

        override fun startActivityForResult(intent: Intent, code: Int) =
            activity.startActivityForResult(intent, code)

        override fun getString(res: Int) = activity.getString(res)
    }

    internal class Fragment(
        private val fragment: androidx.fragment.app.Fragment
    ) : PromptContainer() {

        override val context get() = fragment.requireContext()

        override fun startActivityForResult(intent: Intent, code: Int) =
            fragment.startActivityForResult(intent, code)

        override fun getString(res: Int) = fragment.getString(res)
    }
}
