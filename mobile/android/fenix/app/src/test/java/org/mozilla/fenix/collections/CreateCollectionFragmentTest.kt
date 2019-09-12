/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import androidx.fragment.app.FragmentActivity
import androidx.test.ext.junit.runners.AndroidJUnit4
import assertk.assertThat
import assertk.assertions.isNotNull
import assertk.assertions.isNull
import assertk.assertions.isTrue
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.Robolectric
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class CreateCollectionFragmentTest {
    @Test
    fun `creation dialog shows and can be dismissed`() {
        val activity = Robolectric.buildActivity(FragmentActivity::class.java)
            .create()
            .start()
            .resume()
            .get()

        val fragment = CreateCollectionFragment()

        activity.supportFragmentManager.beginTransaction().apply {
            add(fragment, "test")
            commitNow()
        }

        assertThat(fragment.dialog).isNotNull()
        assertThat(fragment.requireDialog().isShowing).isTrue()
        fragment.dismiss()
        assertThat(fragment.dialog).isNull()
    }
}
