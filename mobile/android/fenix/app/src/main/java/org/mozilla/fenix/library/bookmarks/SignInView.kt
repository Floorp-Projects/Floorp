/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.google.android.material.button.MaterialButton
import kotlinx.android.extensions.LayoutContainer
import org.mozilla.fenix.R

interface SignInInteractor {
    fun onSignInPressed()
    fun onSignedIn()
    fun onSignedOut()
}

class SignInView(
    private val container: ViewGroup,
    private val interactor: SignInInteractor
) : LayoutContainer {

    override val containerView: View?
        get() = container

    val view: MaterialButton = LayoutInflater.from(container.context)
        .inflate(R.layout.component_sign_in, container, true)
        .findViewById(R.id.bookmark_folders_sign_in)

    init {
        view.setOnClickListener {
            interactor.onSignInPressed()
        }
    }

    fun update(signedIn: Boolean) {
        view.visibility = if (signedIn) View.GONE else View.VISIBLE
    }
}
