/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isGone
import androidx.lifecycle.Observer
import androidx.navigation.NavController
import com.google.android.material.button.MaterialButton
import kotlinx.android.extensions.LayoutContainer
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

class SignInView(
    private val container: ViewGroup,
    private val navController: NavController
) : LayoutContainer, Observer<Boolean> {

    override val containerView: View?
        get() = container

    val view: MaterialButton = LayoutInflater.from(container.context)
        .inflate(R.layout.component_sign_in, container, true)
        .findViewById(R.id.bookmark_folders_sign_in)

    init {
        view.setOnClickListener {
            view.context.components.services.launchPairingSignIn(view.context, navController)
        }
    }

    /**
     * Hides or shows the sign-in button. Should be called whenever the sign-in state changes.
     */
    override fun onChanged(signedIn: Boolean) {
        view.isGone = signedIn
    }
}
