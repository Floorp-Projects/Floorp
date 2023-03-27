/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import org.mozilla.fenix.home.mozonline.showPrivacyPopWindow

/**
 * This activity is specific to the Mozilla Online build and used to display
 * a privacy notice on first run. Once the privacy notice is accepted, and for
 * all subsequent launches, it will simply launch the Fenix [HomeActivity].
 */
open class MozillaOnlineHomeActivity : AppCompatActivity() {

    final override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        if ((this.application as FenixApplication).shouldShowPrivacyNotice()) {
            showPrivacyPopWindow(this.applicationContext, this)
        } else {
            startActivity(Intent(this, HomeActivity::class.java))
            finish()
        }
    }
}
