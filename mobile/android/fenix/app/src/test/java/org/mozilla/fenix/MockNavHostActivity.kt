/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.appcompat.app.ActionBar
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import io.mockk.mockk
import org.robolectric.Robolectric

class MockNavHostActivity : AppCompatActivity(), NavHostActivity {

    private val mockActionBar = mockk<ActionBar>(relaxed = true)

    override fun getSupportActionBarAndInflateIfNecessary() = mockActionBar
}

/**
 * Set up an added [Fragment] to a [FragmentActivity] that has been initialized to a resumed state.
 *
 * Variant of [mozilla.components.support.test.robolectric.createAddedTestFragment] that uses
 * a custom Fenix activity to hold the fragment.
 *
 * @param fragmentTag the name that will be used to tag the fragment inside the [FragmentManager].
 * @param fragmentFactory a lambda function that returns a Fragment that will be added to the Activity.
 *
 * @return The same [Fragment] that was returned from [fragmentFactory] after it got added to the
 * Activity.
 */
inline fun <T : Fragment> createAddedTestFragmentInNavHostActivity(
    fragmentTag: String = "test",
    fragmentFactory: () -> T,
): T {
    val activity = Robolectric.buildActivity(MockNavHostActivity::class.java)
        .create()
        .start()
        .resume()
        .get()

    return fragmentFactory().also {
        activity.supportFragmentManager.beginTransaction()
            .add(it, fragmentTag)
            .commitNow()
    }
}
