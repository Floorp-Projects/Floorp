/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.view.View
import mozilla.components.concept.storage.Login

/**
 * An interface for views that can display multiple fillable logins for a site.
 */
interface LoginPickerView {

    var listener: Listener?

    /**
     * Makes the UI controls visible.
     */
    fun showPicker(list: List<Login>)

    /**
     * Makes the UI controls invisible.
     */
    fun hidePicker()

    /**
     * Casts this [LoginPickerView] interface to an actual Android [View] object.
     */
    fun asView(): View = (this as View)

    /**
     * Tries to inflate the view if needed.
     *
     * See: https://github.com/mozilla-mobile/android-components/issues/5491
     *
     * @return true if the inflation was completed, false if the view was already inflated.
     */
    fun tryInflate(): Boolean

    /**
     * Interface to allow a class to listen to login selection prompt events.
     */
    interface Listener {
        /**
         * Called when a user selects a login to fill from the options.
         */
        fun onLoginSelected(login: Login)

        /**
         * Called when the user invokes the "manage logins" option.
         */
        fun onManageLogins()
    }
}
